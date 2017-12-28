#include "c_types.h"
#include "mem.h"
#include "user_interface.h"
#include "ets_sys.h"
#include "osapi.h"
#include "espconn.h"
#include "woz_monitor.h"
#include "spi_flash.h"
#include <ip_addr.h>

#define RAM_SIZE 0x5000
#define INSTRUCTIONS_CHUNK 10000

#define TERM_WIDTH 40
#define TERM_HEIGHT 24

#define SPACE 0x20


static volatile os_timer_t emulator_callback_timer, cursor_timer;

uint8_t computer_ram[RAM_SIZE],
        terminal_ram[TERM_WIDTH * TERM_HEIGHT];

uint16_t load_target_start;

uint32_t current_start,
         current_end,
         loop_counter = 0;

/* Current terminal row and column */
uint8_t term_x = 0,
        term_y = 0,
        cursor_visible = 0,
        cursor_disabled = 0;

struct pia6821 {
        uint8_t keyboard_register;
        uint8_t keyboard_control;
        uint8_t display_register;
        uint8_t display_control;

}   pia = {0};


/* ---------- Function definitions ------------ */

void ICACHE_FLASH_ATTR reset_emulator() {
        term_x = 0;
        term_y = 0;

        ets_memset( computer_ram, 0xff, sizeof(computer_ram) );
        ets_memset( terminal_ram, 0b100000, sizeof(terminal_ram) );
        reset6502();
}

uint8_t read6502(uint16_t address) {
        /* Address in RAM */
        if (address < RAM_SIZE)
                return computer_ram[address];

        /* 4kB of RAM (0x4000-0x5000) is logically mapped to memory bank 0xE000, needed for BASIC. */
        else if ((address & 0xF000) == 0xE000)
                return computer_ram[address - 0xA000];

        /* PIA peripheral interface */
        else if ((address & 0xFFF0) == 0xD010) {
                /* Set keyboard control register to 0 if key was read */
                if (address == 0xD010) {
                        pia.keyboard_control = 0x00;
                }

                return *(&pia.keyboard_register + address - 0xD010);
        }

        /* Address belongs to Woz Monitor ROM (0xFF00 - 0xFFFF) */
        else if ((address & 0xFF00) == 0xFF00)
                return woz_monitor[address - 0xFF00];

        /* Default value */
        return 0xff;
}


void ICACHE_FLASH_ATTR toggle_cursor() {
        uint8_t i;

        cursor_visible ^= 1;
        terminal_ram[term_y * TERM_WIDTH + term_x] = cursor_visible | cursor_disabled ? 0x20 : 0x00;
}


void ICACHE_FLASH_ATTR terminal_write(uint8_t value) {
        /* When changing the terminal_ram, disable cursor first */
        cursor_disabled = 1;

        /* Commit change */
        toggle_cursor();

        /* End of line reached or return pressed */
        if(term_x > 39 || value == 0x0D) {
                term_x = 0;

                if(term_y >= 23) {
                        /* Scroll 1 line up (copy 23 text lines only, blank the last one) */
                        ets_memcpy(terminal_ram, &terminal_ram[TERM_WIDTH], TERM_WIDTH * (TERM_HEIGHT - 1));
                        ets_memset(terminal_ram + TERM_WIDTH * (TERM_HEIGHT - 1), SPACE, TERM_WIDTH);

                }
                else
                        term_y++;

        }

        /* Only printable characters go to terminal RAM. Other characters don't move the cursor either. */
        if (value >= 0x20 && value <= 0x7E) {
                terminal_ram[term_y * TERM_WIDTH + term_x] = value & 0x3F;
                term_x++;
        }

        /* Enable cursor again */
        cursor_disabled = 0;
}


void write6502(uint16_t address, uint8_t value)
{
        if(address < RAM_SIZE) {
                computer_ram[address] = value;
        }

        /* Address belongs to a 4kB bank mapped at (0xE000 - 0xF000), translate it to real RAM 0x4000-0x5000
         * this is needed to run Apple BASIC */
        else if((address & 0xF000) == 0xE000) {
                computer_ram[address - 0xA000] = value;
        }

        /* Write to PIA chip. */
        else if (address == 0xD010) {
                pia.keyboard_register = value;

                /* If a key was pressed, write to keyboard control register as well */
                pia.keyboard_control = 0xFF;
        }
        else if (address == 0xD012) {
                terminal_write(value ^ 0x80);
        }
}


static void ICACHE_FLASH_ATTR emulator_task(os_event_t *events)
{
        current_start = system_get_time();
        exec6502(INSTRUCTIONS_CHUNK);
        current_end = system_get_time();
}


static void ICACHE_FLASH_ATTR dataRecvCallback(void *arg, char *pusrdata, unsigned short lenght){

        char input_character = *pusrdata;

        /* Convert lowercase to uppercase */
        if (input_character > 0x60 && input_character < 0x7B)
                input_character ^= 0x20;

        /* Convert LF to CR */
        else if (input_character == 0x0A)
                input_character = 0x0D;

        /* Convert backspace to "rub out" */
        else if (input_character == 0x7F)
                input_character = '_';

        /* Enable CPU reset from telnet (Ctrl + C) */
        else if (input_character == 0x03) {
                reset_emulator();
                return;
        }

        write6502(0xd010, input_character | 0x80);
}


static void ICACHE_FLASH_ATTR connectionCallback(void *arg){
        struct espconn *telnet_server = arg;
        espconn_regist_recvcb(telnet_server, dataRecvCallback);
}


void tftp_server_recv(void *arg, char *pdata, unsigned short len)
{
        struct espconn* udp_server_local = arg;
        uint8_t ack[] = {0x00, 0x04, 0x00, 0x00};

        if (len < 4)
                return;

        /* Write request, this is the first package */
        if (pdata[1] == 0x02) {
                load_target_start = (computer_ram[0x27] << 8) + computer_ram[0x26];
                if (load_target_start >= 0xE000)
                        load_target_start -= 0xA000;
        }

        /* Data packet */
        else if(pdata[1] == 0x03) {
                /* Copy sequence number into ACK packet and send it */
                ets_memcpy(&ack[2], &pdata[2], 2);

                ets_memcpy(&computer_ram[load_target_start], pdata + 4, len - 4);
                load_target_start += (len - 4);

        }

        espconn_send(udp_server_local, ack, 4);
}


void ICACHE_FLASH_ATTR user_init(void)
{
        uint16_t ui_address;
        struct ip_info ip_address;

        char ssid[32] = "SSID";
        char password[32] = "PASSWORD";

        uart_div_modify(0, UART_CLK_FREQ / 115200);

        uint32 credentials[16] = {0};

        spi_flash_read(0x3c000, (uint32 *)&credentials[0], 16 * sizeof(uint32));

        struct station_config stationConf;

        ets_strcpy(&stationConf.ssid, &credentials[0]);
        ets_strcpy(&stationConf.password, &credentials[8]);

        current_start = system_get_time();

        reset_emulator();
        testi2s_init();

        system_update_cpu_freq( SYS_CPU_160MHZ );

        /* Create a 10ms timer to call back the emulator task function periodically */
        os_timer_setfn(&emulator_callback_timer, (os_timer_func_t *) emulator_task, NULL);
        os_timer_arm(&emulator_callback_timer, 10, 1);

        /* Toggle cursor every 300 ms */
        os_timer_setfn(&cursor_timer, (os_timer_func_t *) toggle_cursor, NULL);
        os_timer_arm(&cursor_timer, 300, 1);

        /* Initialize wifi connection */
        wifi_set_opmode( STATION_MODE );

        wifi_station_set_config(&stationConf);
        wifi_set_phy_mode(PHY_MODE_11B);
        wifi_station_set_auto_connect(1);
        wifi_station_connect();


        /* TFTP server */
        struct espconn *tftp_server = (struct espconn *)os_zalloc(sizeof(struct espconn));
        ets_memset( tftp_server, 0, sizeof( struct espconn ) );
        tftp_server->type = ESPCONN_UDP;
        tftp_server->proto.udp = (esp_udp *)os_zalloc(sizeof(esp_udp));
        tftp_server->proto.udp->local_port = 69;

        espconn_regist_recvcb(tftp_server, tftp_server_recv);
        espconn_create(tftp_server);


        /* Telnet server */
        struct espconn *telnet_server = (struct espconn *)os_zalloc(sizeof(struct espconn));
        ets_memset(telnet_server, 0, sizeof(struct espconn));

        espconn_create(telnet_server);
        telnet_server->type = ESPCONN_TCP;
        telnet_server->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
        telnet_server->proto.tcp->local_port = 23;
        espconn_regist_connectcb(telnet_server, connectionCallback);

        espconn_accept(telnet_server);
}

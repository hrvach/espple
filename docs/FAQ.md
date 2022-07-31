
# FAQ

<br>

##### Backspace doesn't work - it shows `_`

This is by design, Apple 1 output works more like a teletype, there is no going back so this is the "rub out" symbol which tells the computer to disregard the previous symbol.

<br>
<br>

##### I've flashed your bin files, now I'm in a reboot loop

My board is an ESP-12E, and requires DIO flash mode to be set (2 pins used for address and data) as opposed to QIO mode (4 pins used) some other variants use. Try flashing with DIO and see if that helps.

<br>
<br>

##### Wifi doesn't connect

Make sure you've set the credentials like instructed. Try moving the ESP board closer to the access point and configure it to use one of the lower frequency channels (1-6). You can monitor progress using a serial connection (115200, 8N1).

<br>
<br>

##### Why can't I simply use the serial as a keyboard input? This wi-fi thing seems overengineered.

Because the UART0 RX line is already being used as an I2S output, and UART1 exposes TX pin only to the board.

On the plus side, it's totally wireless.

<br>
<br>

##### Program doesn't start after TFTP upload

A packet was probably lost, try again and make sure you have a good wi-fi reception.

<br>
<br>

##### I'd like to use NTSC

Few people asked about the possibility of generating NTSC signal so I've updated the source to support both. Edit generate\_video.c and change #define PAL to #define NTSC.

<br>
<br>

##### I can't find a signal on the TV

**⮩** Your RF input connector and the whole signal path is shielded inside the TV so there should be some sort of antenna plugged in. If you don't have an indoor antenna, a piece of wire will do just fine. Make sure you don't short the tip and ring of your input connector because you won't receive anything. In your TV menu choose analogue TV, choose PAL standard and select channel 4. The esp board emits at 60 MHz which is slightly lower than channel 4 frequency, so you might have to fine tune a bit. Most modern TVs should be able to automatically scan and find the channel for you.

<br>



# Usage

## Flashing

*Flashing the files onto the controller.*

<br>

1.  ***Install a programmer.***
    
    <br>

    ### Linux

    Try **[ESPTool]**.

    <br>

    **Debian**

    ```sh
    sudo apt install esptool
    ```

    **Python**

    ```sh
    pip install esptool
    ```

    **Git**

    ```sh
    gh repo clone espressif/esptool
    ```
    
    <br>
    
    ### Windows

    Try **[Flash Download Tools]**.

    <br>
    <br>

2.  ***Write the binaries.***

    ```shell
    esptool                              \
        --flash_mode dio                 \
        -p /dev/ttyUSB0 write_flash      \
        0x00000 image.elf-0x00000.bin    \
        0x40000 image.elf-0x40000.bin
    ```
    
    <br>

    *`ESP-12E` modules require `dio` as flash <br>
    mode while most other boards use `qio`.*

    <br>
    <br>

3.  ***Set your WiFi credentials.***

    ```sh
    make credentials                    \
        ssid="Your SSID"                \
        password="Your password"
    ```
    
    <br>

    This will generate a `.bin` file containing your <br>
    **credentials** and write it into flash at `0x3C000`.

    **Please keep this in mind and run this command again with** <br>
    **dummy credentials if you intend to borrow / sell your ESP.**

    <br>
    <br>

4.  ***Make the antenna.***

    Simply connect a piece of wire, such as a <br>
    jumper cable to the `RX` / `GPIO 3` pin.

    <br>
    <br>

5.  ***TV Setup***

    Turn your TV on and select `analogue` TV, `PAL` system - `channel 4`.

    *Some manual fine tuning might be required.*

    <br>
    <br>

6.  ***Telnet Connection***

    The **IP** address can be read through <br>
    the cable used to flash the firmware.

    *Make sure your terminal program is set to `115200,8N1`.*
    
    *Press  <kbd>  Enter  </kbd>  several times after connecting.*

<br>
<br>

## Loading the software

First you need to tell the built-in **TFTP** <br>
server where to store your program.

The location is determined by what <br>
path the built-in monitor last used.

The default location after a reboot is `\`. <br>
For **BASIC**, the target address is `0xE000`.

Simply type `E000` and press enter, this <br>
will specify the upload target address.

Then run the **TFTP** client with adjusted **IP** on <br>
your computer and upload the binary file:

```sh
atftp -p -l software/basic-0xE000.bin 192.168.1.2
```

After the **TFTP** client finishes, simply run <br>
the program from the target location by <br>
typing the location followed by the letter `R`.

```basic
E000R
```

This should greet you with `>` in response.

<br>

## Rebooting

To have the emulator **Reboot**, press  <kbd>  Ctrl + C  </kbd>

This will neither drop the **WiFi**, nor the **TelNet** connection.

<br>


<!----------------------------------------------------------------------------->

[Flash Download Tools]: http://espressif.com/en/support/download/other-tools
[ESPTool]: https://github.com/espressif/esptool



<div align = center>

# How Does It Work?

<br>

![Screen]

</div>

<br>
<br>

## Communication

To **ESP** internally generates a `High Frequency Modulated Video Signal` <br>
via `I2S` / `DMA` which can be transmitted with a just a piece of wire.

Despite this signal being `extremely weak`, <br>
it is still sufficient to be picked up by you **TV**

<br>
<br>

## I2S / DMA

The **ESP** supports **I2S**, `Inter-IC Sound`, a <br>
way of interconnecting digital audio devices.

Utilizing **DMA** to transfer data, a steady stream of bits <br>
can be transmitted, without occupying the processor.

Once all data of the current batch have been sent, <br>
the process is interrupted to request new data.

<br>
<br>

## Signal

Now one can generate patterns of bits to produce meaningful <br>
waveforms that a **TV** can interpret as `Video Signal`.

*As I live in Europe, the chosen video system is a `625 Lines CCIR System B`.* <br>
*➞ `PAL B` without color*

<br>

![Spectrum]

<br>


<!----------------------------------------------------------------------------->

[Spectrum]: ../Resources/Spectrum.png
[Screen]: ../Resources/Screen.png

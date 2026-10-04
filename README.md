# Night City Custom Firmware for Stargate MSX

Custom ESP8266 firmware for the [Stargate MSX](https://github.com/leomanes/stargate) by [Leo Manes](https://github.com/leomanes).

If you aren't familiar with the Stargate MSX, it is a fantastic MSX compatible made using the [MSXgoauldSD_tn20k](https://github.com/jabadiagm/MSXgoauldSD_tn20k) as a base. You get a lot of modcons including HDMI output and SD card filesystem.

More details can be found in the [Stargate MSX forum post](https://www.msx.org/forum/msx-talk/hardware/stargate-msx-compatible-computer).

I had an issue with the stock firmware with the WiFi menu locking up the menu system. So I worked with ChatGPT to create an alternate firmware(_not necessarily better!_) that met my personal wants and needs. 

I've used these little OLEDs for years and they can definitely be burned in so I did add a screen timeout so that I didn't have to worry about the screen being burned in.

Menuing has been tweaked to be simpler for myself to just bing bong around and change things without too many button presses.

I wanted WiFi to be toggle-able by the switch on the back of the board so I could do it willy nilly. Also I wanted some basic CPU overclocking to be able to be done using the other switch on the back.

Flashing the chip for myself was tested in both Windows and MacOS using the standard Arduino IDE.

## Features

- CPU dashboard showing configured speed and Wi-Fi/Bluetooth status.
- Stock and 1.5× Turbo CPU modes.
- SW3 toggles Stock/Turbo and wakes the CPU dashboard.
- Stock CPU speed on startup.
- Wi-Fi control through the menu and SW4.
- Wi-Fi LED and dashboard indicator blink while connecting.
- Bluetooth module power control.
- RGB lighting with solid colors, breathing effects, and color cycling.
- ROYGBIV color selection, adjustable brightness, and LED count.
- Save Settings option for lighting, Bluetooth, and Wi-Fi startup preferences.
- Automatic return to the CPU dashboard after 10 seconds of menu inactivity.
- OLED sleep after 30 seconds of inactivity.
- SW6, SW7, or SW8 wakes the OLED onto the CPU dashboard.
- Responsive menu operation while Wi-Fi is active.

## Installation

### 1. Install ESP8266 support

Install Arduino IDE, then add this URL under **Preferences → Additional Boards Manager URLs**:

```text
https://arduino.esp8266.com/stable/package_esp8266com_index.json
```

Open **Boards Manager**, search for **esp8266 by ESP8266 Community**, and install version **3.1.2**.

### 2. Apply the required core customizations

The networking firmware requires the supplied ESP8266 core customizations. From the full source package's root directory, run:

```sh
python3 tools/install_core_customizations.py "/path/to/esp8266/3.1.2"
```

Typical core installation locations:

- **Windows:** `%LOCALAPPDATA%\Arduino15\packages\esp8266\hardware\esp8266\3.1.2`
- **macOS:** `~/Library/Arduino15/packages/esp8266/hardware/esp8266/3.1.2`
- **Linux:** `~/.arduino15/packages/esp8266/hardware/esp8266/3.1.2`

The script backs up replaced files before applying the customizations.

If applying these files manually, merge them into the existing core installation. **Do not replace entire library folders**, since the customization package contains only selected files.

Reinstalling or updating the ESP8266 core may overwrite these changes.

### 3. Install the libraries

Using Arduino Library Manager, install:

- **Etherkit Si5351** — version 2.x API
- **Adafruit SSD1306**
- **Adafruit GFX Library**
- **Adafruit BusIO**
- **Adafruit NeoPixel**

Accept any dependency installation prompts.

### 4. Prepare the sketch

Open:

```text
firmware/Stargate_Night_City/Stargate_Night_City.ino
```

The sketch folder and `.ino` file must both be named `Stargate_Night_City`. Keep all supplied supporting headers in that folder.

If installing an update package, follow its included instructions and replace only the specified files. Update packages are not necessarily complete standalone sketches.

### 5. Configure the hardware

Review `BoardConfig.h` before compiling.

For the configuration used by this project—verified v2.1.3 clock wiring, U2 installed, and a 25 MHz / 10 pF Si5351 module—the settings are:

```cpp
#define STARGATE_CLOCK_WIRING_VERIFIED 1
#define STARGATE_U2_FITTED 1
#define STARGATE_SI5351_CRYSTAL_HZ 25000000UL
#define STARGATE_SI5351_LOAD_PF 10
```

Use values appropriate for your actual hardware.

For this configuration:

- JP2 and JP3 must be open.
- Leave U2 fully installed, including pin 5.
- The firmware leaves Si5351 CLK1 disabled because U2 supplies the PSG clock.
- Turbo changes only the CPU clock.

When updating a working installation, keep your existing `BoardConfig.h`.

### 6. Select the Arduino settings

Under **Tools**, select:

| Setting | Value |
|---|---|
| Board | Generic ESP8266 Module |
| CPU Frequency | 160 MHz |
| Flash Size | Match the installed ESP module and existing filesystem layout |
| Port | Your serial programmer's port |
| Erase Flash | Only Sketch, when updating an existing installation |

The project's 4 MB module configuration uses **4 MB flash with a 2 MB filesystem**. Use this only if it matches your module.

The **160 MHz** setting controls the ESP8266 itself, not the MSX CPU.

Keep your known-working flash mode, upload speed, and reset method when updating.

### 7. Compile and flash

1. Click **Verify** to compile the sketch.
2. Connect your ESP8266 programmer using **3.3 V UART logic** and a common ground.
3. Enter the ESP8266's serial bootloader using your established flashing procedure.
4. Upload directly from Arduino IDE, or use **Sketch → Export Compiled Binary** and flash the exported application binary with your ESP8266 flashing tool.
5. Return the ESP8266 to normal boot mode and restart the board.

When using an external flasher, the Arduino-generated ESP8266 application binary is written at offset `0x00000`.

Preserve the existing filesystem when updating so saved settings remain available. Avoid powering the board from two conflicting power sources.

### 8. Check operation

After flashing:

- Confirm the MSX boots normally.
- Confirm the OLED opens on the CPU dashboard in **Stock** mode.
- Press **SW3** to switch between Stock and 1.5× Turbo.
- Confirm SW3 wakes the OLED and displays the CPU dashboard.
- Check Wi-Fi using **SW4** or the menu.
- Check Bluetooth and RGB controls.
- Confirm the menu returns home after 10 seconds idle and the OLED sleeps after 30 seconds.
- Use **Save Settings** to preserve supported preferences.

The firmware starts at Stock speed after every ESP8266 restart. Turbo is not restored automatically at boot.

## Controls

- **SW3:** Toggle Stock/Turbo and wake the CPU dashboard.
- **SW4:** Toggle Wi-Fi.
- **SW6 / SW7:** Navigate menus or adjust values.
- **SW8:** Select, toggle, or finish editing.

## Notes

- Turbo changes only the CPU clock; SYS and PSG remain at stock speeds.
- Turbo stability depends on the hardware and software in use.
- Save Settings must be selected explicitly to preserve supported preferences.

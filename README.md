# ESPHome Universal A/C IR Control (Using BC7215)

This ESPHome project turns an ESP32 module plus a BC7215 module into a universal A/C IR remote that can be connected to Home Assistant. It supports one-step pairing with almost any air conditioner. You will be able to control the air conditioner through Home Assistant, and control operations made with the original IR remote can also be synchronized back to Home Assistant.

This project can use any ESP32 module. You only need to change the hardware settings in the `bc7215_ac_ctrl.yaml` file.

Platform definition section:

```yaml
esp32:
  variant: esp32c3
```

BC7215 module connection definition section:

```yaml
climate:

- platform: bc7215_ac
  id: ac_controller
  name: "Air Conditioner"

  bc7215_uart_num: 1
  bc7215_tx_pin: GPIO3
  bc7215_rx_pin: GPIO4
  bc7215_busy_pin: GPIO1
  bc7215_mod_pin: GPIO0
```

**Note: Do not choose a UART that conflicts with the download/programming port. Also avoid using GPIO pins that conflict with other hardware, such as an LCD, LEDs, buttons, etc.**

## Installation

1. Clone the project. Because this project depends on the AC control library project and examples at <https://github.com/bitcode-tech/bc7215_ac_lib>, you cannot download it as a ZIP file. You must use the `git clone --recursive` command to fully obtain the dependent library:
   
   ```bash
   git clone --recursive https://github.com/timj-code/bc7215_ac_esphome.git
   ```

2. After cloning the project locally, run the included Python utility to create a directory environment file:
   
   ```bash
   cd esphome_ac_control
   python3 tools/prepare_esphome.py
   ```
   
   After running it, a `local_paths.yaml` file will be created in the project root directory. It contains the absolute path of the project directory. Its contents will be something like the following:
   
   ```yaml
   bc7215_project_dir: "/home/xxx/projects/esphome_ac_control"
   ```
   
   Or, under Windows, it may look like this:
   
   ```yaml
   bc7215_project_dir: "C:/Users/xxx/esphome_ac_control"
   ```
   
   You can also create this file manually, but make sure both the file name and directory path are correct.

3. Modify the processor and I/O pin settings in `bc7215_ac_ctrl.yaml` according to your own hardware.

4. Run the following command in the project root directory:
   
   ```bash
   esphome run bc7215_ac_ctrl.yaml
   ```
   
   Or run:
   
   ```bash
   esphome dashboard ./
   ```
   
   Then install it from the browser. The first installation must be done through a USB-to-serial connection. After that, OTA can be used, so the device does not need to be physically connected to the computer.

5. If you got an error says there's no WiFi information, rename the `secrets.yaml.temaplate` file to `secrest.yaml` and replace the file contents with your WiFi name and password.

## Usage

After the hardware is connected and the firmware is compiled and installed on the ESP32, an air-conditioner device will appear in Home Assistant. The device contains several entities: an air-conditioner controller, two buttons, and one Text Sensor used to display necessary status information.

Before controlling the air conditioner, you must pair it with the AC. Pairing only requires one step and does not require knowing the brand or model, because the AC control library automatically identifies the protocol used. The pairing operation is to send an IR remote control signal to the BC7215 module.

**The transmitted signal must be from the remote control in cooling mode at 25°C.** 

> (or 78°F if the AC is  Fahrenheit, for ACs in Fahrenheit, you may also need to change yaml file. The Fahrenheit mode is not tested as I don't have one, those who have are welcomed to give a feedback)

A reliable method of pairing is to first set the remote control to cooling mode at 25°C first, and then use the **Fan Speed** button for pairing. The device information page shown in Home Assistant looks roughly like this:

![Buttons and status display](./img/Home%20Assistant1.png)

Press the **Pair Button**, and the content of the Text Sensor will change to:

> Pairing: press Fan with 25°C/Cool mode

At the same time, the blue LED on the BC7215 module will light up. At this point, you can press the remote control button to pair. Pairing is usually completed almost instantly, although the display in Home Assistant may have a slight delay. After pairing is complete, the displayed content will become:

> AC Paired

At this point, you should be able to control the air conditioner through Home Assistant. The blue LED on the BC7215 module will remain on, because the program keeps it in receive mode continuously. In this way, the BC7215 decoding function can synchronize operations made with the original remote control back to Home Assistant. 

![](./img/Home%20Assistant2.png)

#### Purpose of the Alt Button:

According to the BC7215 AC library documentation, a small number of air conditioners may fail to pair, or may pair successfully but still cannot control the AC. **This button is only intended to be used in such situations.** In my program design, each press switches to another built-in special protocol and sends a test signal. If you find that the air conditioner responds after switching to a certain protocol, that should be the setting capable of controlling your air conditioner. Since I have not encountered an air conditioner that cannot be controlled, I have not been able to test this feature myself. Feedback from users who encounter this situation would be appreciated.

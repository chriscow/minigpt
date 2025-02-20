# MiniGPT

<img src="https://chriscow-assets.sfo3.cdn.digitaloceanspaces.com/minigpt/minigpt-closeup.jpg" height="400">

MiniGPT is a charming and compact desktop toy that brings the power of OpenAI's GPT-4o to a tiny vintage-style computer. Built to run on a Lilygo T-Watch with an attached keyboard, this project combines retro aesthetics with modern AI capabilities, making it a delightful addition to your workspace.

## Overview

MiniGPT resembles a miniature vintage computer, reminiscent of the classic Commodore PET. Powered by an ESP32 microcontroller, it connects to the OpenAI GPT-4o API, allowing you to chat and interact with the AI through this adorable little device. It's a fun and functional project that blends nostalgia with cutting-edge technology.

## Features

- **Compact Design**: Fits perfectly on your desk as a tiny vintage-style computer.
- **OpenAI GPT-4o Integration**: Connects to the OpenAI API for AI-powered conversations.
- **Customizable**: Easily configure your API key, model, and branding.
- **PlatformIO Support**: Built using PlatformIO for seamless development and deployment.

## Getting Started

<img src="https://chriscow-assets.sfo3.cdn.digitaloceanspaces.com/minigpt/minigpt-desktop-toy.jpg" height="400">

### Prerequisites

- [Visual Studio Code (VSCode)](https://code.visualstudio.com/)
- [PlatformIO Extension](https://platformio.org/install/ide?install=vscode) for VSCode
- A [Lilygo T-Watch with a keyboard](https://www.aliexpress.us/item/3256804521965718.html)
- An OpenAI API key

### Building the Project

1. **Clone the Repository**:
   ```bash
   git clone https://github.com/chriscow/MiniGPT.git
   cd MiniGPT
   ```

2. **Install Dependencies**:
   - Open the project in VSCode.
   - Ensure the PlatformIO extension is installed.

3. **Configure the Project**:
   - Create a `secrets.ini` file in the project root with the following content:
     ```ini
     [env:ttgo-t-watch]
     build_flags = 
         -D OPENAI_API_KEY="your-openai-key-here"
         -D OPENAI_MODEL="gpt-4o-mini"
         -D COMPANY_NAME="Company or Brand"
         -D NAME="MiniGPT"
     ```
   - Replace the placeholders with your actual OpenAI API key, desired model, and branding details.

4. **Build and Upload**:
   - Use PlatformIO to build and upload the project to your Lilygo T-Watch.

### Usage

Once the firmware is uploaded, power on your Lilygo T-Watch. You will need to use your phone or computer to join the "MiniGPT" Wifi hotspot. Once you join, you will be presented with a web UI to configure the SSID and password so it can join your Wifi access point of choice.  Once configured, it will reboot.  You will see a welcome message from OpenAI and you are ready to start chatting with MiniGPT! 

## License

This project is licensed under the Apache 2.0 License. See the [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! If you have any ideas, improvements, or bug fixes, feel free to open an issue or submit a pull request.

## Acknowledgments

- **OpenAI**: For providing the GPT-4o API.
- **Lilygo T-Watch**: For the hardware platform that makes this project possible.
- **PlatformIO**: For the excellent development environment.

---

Enjoy your MiniGPT and happy chatting! 🚀

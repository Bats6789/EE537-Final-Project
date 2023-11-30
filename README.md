# EE537-Final-Project
A wireless scale capable of reporting weight to both a bluetooth app and a ThingSpeak cloud server.

## Dependencies
The following libraries are required for this project:
* Load Cell Library - [HX711](https://github.com/bogde/HX711)
* LCD Library - [LiquidCrystal_I2C](https://github.com/ribasco/new-liquidcrystal)
* ThingSpeak - [ThingSpeak](https://github.com/mathworks/thingspeak-arduino)

## Instalation instructions
The instalation instructions can be found in [Instructions](/Instructions) if you prefer the word file.

### Android Install
1. Locate the EE537App.apk file in [AndroidApp/App](/AndroidApp/App)
2. Start the install process by tapping the apk and clicking install.
![Android Install Prompt](/Images/AndroidInstallPrompt.png)
3. If installation permissions are requested, accept them.

### ThingSpeak Setup
1. Sign into ThingSpeak using your MathWorks account credentials.
2. Click **Channels > My Channels**.
![ThingSpeak Channel](/Images/ThingSpeakChannel.png)
3. On the Channels page, click **New Channel**.
4. Name the new channel **Measurement**.
5. Name Field 1 **Weight**. 
![ThingSpeak Channel Name](/Images/ThingSpeakChannelName.png)
6. Click **Save Channel** at the bottom of the settings.
7. The following screen should be visible. Click API Keys.  
![ThingSpeak API Keys](/Images/ThingSpeakAPIKeys.png)
8. Note your **Channel ID** and your **Write API Key**. They will be needed for setup.

### Project Setup
Project Setup
1.  Follow the circuit schematic for setting up the hardware. These instructions will focus on the software side of things.
2.  Load ESPCalibration.ino onto the ESP32. Instructions will be displayed on the LCD.
3.  Note the calibration number obtained.
4.  Navigate to ESPSoftware.ino to change the following parameters:
    a.  On line 23, replace <factor> with your calibration number obtained in step 3. 
    b.  On line 26, modify <WiFi-Name> to your WiFi networks name.
    c.  On line 27, modify <WiFi-Password> to your WiFi password.
    d.  On line 30, modify <API-KEY> with your write API-KEY obtained in the ThingSpeak setup.
    e.  On line 31, modify <channel-ID> to contain your channel ID obtained in the ThingSpeak setup.
5.  Save and load the file onto the ESP32. The device is now ready for use.

### Test Instructions
1. Upon opening the app, you will be greeted with a profile creation screen.
![Android Test Instructions 1](/Images/AndroidTestInstruction1.png)
2. Either enter your name manually or select a contact to populate the fields.
3. Tap the checkmark after the fields are populated.
![Android Test Instructions 2](/Images/AndroidTestInstruction2.png)
4. Tap the Wi-Fi
![Android Test Instructions 3](/Images/AndroidTestInstruction3.png)
5. Accept all permission
![Android Test Instructions 4](/Images/AndroidTestInstruction4.png)
![Android Test Instructions 5](/Images/AndroidTestInstruction5.png)
6. The device should connect as indicated by the green dot on the Wi-Fi symbol.
![Android Test Instructions 6](/Images/AndroidTestInstruction6.png)
7. Step on scale to acquire weight.
![Android Test Instructions 7](/Images/AndroidTestInstruction7.png)

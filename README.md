This a template for creating a project using the Cheap Yellow Display (CYD), EEZ Studio, and LVGL 9.x in PlatformIO/VSCode.  

You only need to add callbacks for your actions and your applicatyion code.

  This assumes you already have LVGL working with PlatformIO/ESP32
  1. Create a new PlatformIO/VSCode project using this template
  2. Create UI with EEZ Studio
    - Remember to set both screen width/height and Depth=16 bits as used by ESP32
    - Create the GUI in EEZ Studio
    - BUILD the project in EEZ Studio
      - This will generate ui code to copy to PlatformIO/VSCode
    - Copy ui directory from EEZ Studio to the PlarformIO/VSCode project lib directory
    - Be sure the  ui_init();  is added to setup()
    - Compile & run the program

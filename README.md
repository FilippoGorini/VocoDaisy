# VocoDaisy


**VocoDaisy** is a port of the original *mda Talkbox* plugin, created by **Paul Kellett** at [Maxim Digital Audio](http://mda.smartelectronix.com/), to the Daisy Seed platform. The original source code was open-sourced to [SourceForge](https://sourceforge.net/projects/mda-vst/) in 2015 and is available at the following [GitHub repo](https://github.com/pluginguru/mda-effects).

This project extracts and refactors the *mda Talkbox* algorithm — originally written for VST2 hosts — into a standalone C++ class suitable for the [Daisy](https://www.electro-smith.com/daisy) audio platform.

The **mda Talkbox** effect is a *Linear Predictive Coding (LPC) vocoder*, similar to the one used in the *Digitech Talker* pedal — famously employed by *Daft Punk* for their characteristic robotic vocal sounds.


## Building the Project

### 🧰 What You Need

* [Daisy Toolchain](https://daisy.audio/tutorials/toolchain-windows/) installed and set up.
* [DaisyExamples](https://github.com/electro-smith/DaisyExamples) repo, which contains examples and, most importantly, the [libDaisy](https://github.com/electro-smith/libDaisy) and [DaisySP](https://github.com/electro-smith/DaisySP) libraries, which are needed to build the project. Make sure to put it in the same folder where you cloned the VocoDaisy repo.


### 🎛️ Building for Daisy (Firmware Build)

You can build the firmware version directly in VS Code:

1. Open the VocoDaisy project folder in VS Code.
2. Press **Ctrl + Shift + B** to build it with the Daisy toolchain.

If you get errors regarding libDaisy and DaisySP, make sure to have built the libraries at least once. To do so:

1. Press **Ctrl + Shift + B**.
2. Select the **Tasks: Run Task** command.
3. Then select the **build_all** task (this will build the libraries contained in the DaisyExamples folder).

For this to work, your VocoDaisy folder and the DaisyExamples one should be in the same directory.


### 💻 Building and Running the Desktop Test Version

If you want to try the effect on your computer before flashing it to Daisy, you can build and run the test version:

1. Open a terminal and go to the project root folder.

2. Build the test executable:

   ```bash
   make test
   ```

3. Put your WAV files (the modulator and carrier) inside the `test` folder.

4. Run the executable:

   ```bash
   ./test
   ```

   If you just run it like that, it’ll automatically use:

   * **mod.wav** → modulator input
   * **car.wav** → carrier input
   * **out.wav** → output file
   * 48 → blockSize

   You can also pass your own files and block size if you want:

   ```bash
   ./test <modulator.wav> <carrier.wav> <output.wav> <blockSize>
   ```

   Example:

   ```bash
   ./test vocals.wav synth.wav vocoded.wav 64
   ```


## Notes

* Make sure your `.wav` files are in the `test` folder before running the executable.
* The test build is just meant for checking the algorithm and doing some quick experiments — real-time performance depends on your computer.

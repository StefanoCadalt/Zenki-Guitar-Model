# Zenki Guitar Model

> **Disclaimer:** This repository is a clean showcase of the final build for portfolio purposes. The software was originally developed as a collaborative academic project. The original repository with the full development history can be found here: [https://github.com/ENRICOSCHI/ModelloFisicoChitarra].

## Project Information

* **Role:** Audio Programmer, DSP Developer
* **Team:** 3
* **Year:** 2026
* **Production Time:** One month
* **Context:** Academic Project (Circuits and algorithms for sound design Course)
* **Type:** Audio Plugin / Software Synthesizer
* **Core Tech:** C++, JUCE Framework

---

## General Project Description

Zenki Guitar Model is a **real-time VST3 synthesizer plugin** developed in C++ utilizing the JUCE framework. The software leverages physical modeling synthesis to mathematically recreate the acoustic behavior of a plucked string instrument. 

At its core, the plugin implements an advanced **Karplus-Strong algorithm**, enhanced with a Jaffe-Smith all-pass filter to guarantee micro-tonal tuning accuracy through fractional delay.

### Key Technical Features

* **Custom DSP Effects Chain:** Integrates hyperbolic soft-clipping distortion, phase modulation, a ring-buffer delay, and stereo reverberation.
* **Thread-Safe Architecture:** Engineered with a strict lock-free system, utilizing atomic variables to bridge the high-priority Audio Thread and the UI Thread.
* **High Performance:** Ensures zero audio dropouts while handling complex signal processing in real-time.
* **Dynamic Graphical Interface:** Simultaneously drives a responsive UI featuring physically consistent, **Bezier-curve-based string animations**.

---

## Credits & Collaborators

This project was developed by:
* **Stefano Cadalt:** - [https://github.com/StefanoCadalt]
* **Enrico Fiore:**  - [https://github.com/ENRICOSCHI]
* **Davide Pica:** - [https://github.com/duddy-bin]

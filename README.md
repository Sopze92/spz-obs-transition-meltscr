# DooM's Melting screen transition plugin for OBS

Extended re-implementation of the DooM (1991) screen melt effect as a scene transition for OBS.

<h4 align="center">
  <img src="https://img.shields.io/badge/OBS-30-blue.svg?style=flat" alt="supports obs version 30">
  <img src="https://img.shields.io/badge/Windows-0078D6?style=flat&logo=data%3Aimage%2Fpng%3Bbase64%2CiVBORw0KGgoAAAANSUhEUgAAABMAAAATCAYAAAByUDbMAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAADsIAAA7CARUoSoAAAAAYdEVYdFNvZnR3YXJlAFBhaW50Lk5FVCA1LjEuOBtp6qgAAAC2ZVhJZklJKgAIAAAABQAaAQUAAQAAAEoAAAAbAQUAAQAAAFIAAAAoAQMAAQAAAAIAAAAxAQIAEAAAAFoAAABphwQAAQAAAGoAAAAAAAAA8nYBAOgDAADydgEA6AMAAFBhaW50Lk5FVCA1LjEuOAADAACQBwAEAAAAMDIzMAGgAwABAAAAAQAAAAWgBAABAAAAlAAAAAAAAAACAAEAAgAEAAAAUjk4AAIABwAEAAAAMDEwMAAAAADMbhZ8SlPoHAAAADFJREFUOE9joCZgBBH%2FgQDMwwEYgYAYNUxQNlXAqGGkg8FrGFXBaKIlHYwaNqCAgQEAz4kQH5TYSpkAAAAASUVORK5CYII%3D&logoColor=white">
  <img src="https://img.shields.io/badge/Mac_OS-e8e8e8?style=flat&logo=apple&logoColor=black">
  <img src="https://img.shields.io/badge/Linux-FCC624?style=flat&logo=linux&logoColor=black">
</h4>

<h4 align="center">
  <a href="https://patreon.com/sopze">
    <img src="https://img.shields.io/badge/Patreon-f86b59?style=flat&logo=patreon&logoColor=white">
  </a>
  <a href="https://buymeacoffee.com/sopze">
    <img src="https://img.shields.io/badge/BuyMeACoffee-ffdd00?style=flat&logo=buymeacoffee&logoColor=black">
  </a>
  <br>
  <a href="https://sopze.com">
    <img src="https://img.shields.io/badge/Personal_site:-At_Sopze's-BAED20?style=flat">
  </a>
</h4>

## Features
* Works in all 4 directions: Up | Down | Left | Right
* Configurable amount of slices and the variation between them
* Configurable overal size of the effect as a screen percentage
* 3 Random generation modes: DooM, Fixed and Dynamic (see below)
* 3 Audio transition modes: Smooth | Swap | Mute

#### Random generation modes
* DooM: Generates screens with the same exact melting pattern that DooM had
* Fixed: Generates a random DooM-alike pattern once, reuses it forever
* Dynamic: Generates a random DooM-alike pattern every time transition is triggered

_the Fixed Random Tables are written to disk so you wont lose them between sessions_

#### Audio transition modes
* Smooth: Generic Ease-in-out interpolation
* Linear: Basic linear interpolation
* Swap: Audio will swap instantly upon reaching a custom defined point
* Mute: Fully mutes everything until the transition finishes

## Localization

The plugin is currently available in 8 languages

Manually written:
* English (US)
* Spanish (Spain)

Automatic translations (may contain errors):
* Portuguese (Portugal)
* German
* French
* Italian
* Russian
* Japanese
# iOS 16.5 on the OnePlus 9 Pro

Apple's open-source **XNU kernel** (`xnu-8796.121.2`, iOS 16.5) and the **unmodified iOS 16.5 user space** of the
iPhone X — launchd, backboardd, SpringBoard, the stock apps — running on a **OnePlus 9 Pro** (`lemonadep`,
Snapdragon 888 / SM8350).

The port is built on [HackiOS](https://github.com/oosexgang/hackios-xnu-kernel) (OOSEX GANG), which does the same on
the POCO X5 Pro. This repository is the project's hub: the notes, a setup script and the earlier work it grew out
of. The code is on the `lemonadep` branches of two forks:

| Repository | What |
|---|---|
| **furangelz/ios-sweet** (this one) | notes (`NOTES.md`), `setup.sh`, the earlier ios-sweet work in `legacy/sweet/` |
| [furangelz/hackios-xnu-kernel](https://github.com/furangelz/hackios-xnu-kernel/tree/lemonadep) `lemonadep` | the kernel and its drivers, the EFI loader, the UEFI port, `xl`; the port's page: [docs/lemonadep.md](https://github.com/furangelz/hackios-xnu-kernel/blob/lemonadep/docs/lemonadep.md) |
| [furangelz/hackios-xnu-userland](https://github.com/furangelz/hackios-xnu-userland/tree/lemonadep) `lemonadep` | init, injected libraries, launchd jobs, the image builder |

> Experimental research, not a phone OS. **Nothing is flashed**: the phone `fastboot boot`s a UEFI, the system runs
> from RAM, and a reboot brings back Android. No Apple files are in these repositories: the iOS system image is
> made on your machine from Apple's own IPSW. Not affiliated with or endorsed by Apple or OnePlus.

## State (2026-10-24)

| Works | Not yet |
|---|---|
| Boot to the lock screen and home screen, apps, Control Center, Settings | Wi-Fi, Bluetooth, cellular |
| Touchscreen (Synaptics S3908), power and volume keys | Sound (the audio DSP runs; the speaker path is not ported) |
| 7 CPU cores, the X1 at 2.84 GHz | Camera, sensors, alert slider |
| Adreno 660 GPU through Mesa: CoreAnimation draws with OpenGL ES | Fast charging (needs OnePlus's VOOC handshake) |
| Tear-free output: page flips in the display engine | Charging while the phone waits in the loader |
| Brightness from iOS's slider; dims and turns off with the screen | Data that survives a reboot |
| Battery level from the phone's fuel gauge; charging (500 mA) by the phone's own audio DSP firmware | |

## How it boots

```
OnePlus ABL (bootloader unlocked)
  └─ fastboot boot: Mu-Silicium UEFI with the 9 Pro's package (lemonadepPkg)
       └─ xnuloader (EFI app, stays on core 0): receives the kernel and the system image over USB
            └─ XNU on the other 7 cores ─ iOS 16.5 from RAM ─ the log back over USB when the run ends
```

## Getting started

Needs: Linux x86-64, a OnePlus 9 Pro with an unlocked bootloader, about 20 GB free, the tools that
`hackios-xnu-kernel/common/setup.sh` checks for (python3, git, curl, 7-Zip, fastboot, adb; clang/lld and theos only
to rebuild the kernel and the loader).

```sh
./setup.sh                                  # clones both forks (lemonadep) into hackios-xnu-kernel/
cd hackios-xnu-kernel
ios165/fetch.sh                             # once: iOS 16.5 out of Apple's IPSW
# the phone's own firmware (GPU, audio DSP) into fw-gpu/ and fw-audio/: docs/lemonadep.md
common/build.sh lemonadep                   # kernel (with theos), loader, images
XL_DEVICE=lemonadep ./xl enter              # the phone into the loader
XL_DEVICE=lemonadep ./xl ramdisk            # the system image, once per loader session
XL_DEVICE=lemonadep XL_SECS=600 ./xl run    # iOS for ten minutes; the log comes back to logs/
```

Read the cautions in [docs/lemonadep.md](https://github.com/furangelz/hackios-xnu-kernel/blob/lemonadep/docs/lemonadep.md)
first: the battery (nothing charges between runs), and never pulling the USB cable during a run.

## Layout of this repository

```
README.md        this page
NOTES.md         working notes: the current state first, then the bring-up log
AGENTS.md        instructions for coding agents working on the project
setup.sh         the workspace: the two forks, checked out into hackios-xnu-kernel/
legacy/sweet/    ios-sweet, where this started: iOS 16 on the Redmi Note 10 Pro (sweet) with its own shim and
                 XNU patch series, and the first lemonadep attempts with that shim (its README and notes)
```

## Credits

- [HackiOS](https://github.com/oosexgang) (OOSEX GANG): the kernel, loader, drivers and user space this port is
  built on.
- [ios-sweet](https://github.com/dipa-studio/ios-sweet): the project this repository was forked from.
- [Project Silicium / Mu-Silicium](https://github.com/Project-Silicium/Mu-Silicium): the UEFI, and the OnePlus 9
  package the 9 Pro's is made from.
- [popovluka/ios-lemonadep](https://github.com/popovluka/ios-lemonadep): the S3908 touch report configuration.

iOS, iPhone and Apple are trademarks of Apple Inc.; OnePlus is a trademark of OnePlus Technology.

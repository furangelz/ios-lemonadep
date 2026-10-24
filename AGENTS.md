# Working on the OnePlus 9 Pro (lemonadep) port

- Read `NOTES.md` before starting: its first section is the current state, where the code is, how to build and
  run, the open problems and the safety rules; the bring-up log follows.
- The user switches agents frequently. Update the handoff at the top of `NOTES.md` after each milestone and before
  stopping. Record commits (of every repository involved), exact build/boot results and unfinished work.
  Distinguish observations from guesses.
- The code is in `hackios-xnu-kernel/` (the kernel repository, `lemonadep` branch, with the userland repository
  overlaid in the same tree: `common/ugit` for that one). Edit the XNU tree in
  `hackios-xnu-kernel/xnu-src/xnu-8796.121.2`, then run `xnu/export.sh` before committing. Commit completed changes
  in the repository they belong to; this hub only holds notes, `setup.sh` and `legacy/`.
- This machine: host tools (compilers, git commit/push, gh, fastboot, adb) only through `flatpak-spawn --host`;
  the host cannot see the sandbox's `/tmp`.
- Device bring-up uses temporary `fastboot boot` images; nothing is flashed. Preserve the Android installation and
  the partition backups. The user approved (2026-10-15) using internal UFS for iOS and said Android may be
  deleted: an iOS root goes into the existing `userdata` partition (no GPT changes). Keep bootloaders, recovery,
  modem, persist and the backups intact, and confirm each destructive step with the user before running it.
- Ask the user before new TrustZone, PMIC or power steps. Never write to a charger chip; the DSP battery manager's
  limits are only set with the opt-in boot-arg `charge_ma`.
- Watch the battery in every log (`battery: gauge ok`); the loader does not charge. Do not ask for the USB cable
  to be pulled during a run unless the run's result is read off the panel (the loader's USB does not come back).
- Keep device dumps, serial numbers, the phone's firmware (`fw-*/`), logs and downloaded Apple artifacts out of git.

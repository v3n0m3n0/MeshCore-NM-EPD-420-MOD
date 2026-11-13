# Releasing RockBase NM-EPD-420 MOD Firmware

This fork uses unified `0x0` multi-role builds and GitHub Releases tagged with `v<upstream>-mod-rev<X.Y>` (for example, `v1.18.0-mod-rev0.3`).

---

## Pre-Release Checklist

1. **Build & Verify Anonymized Suite:**
   ```powershell
   .\build_and_release.ps1 -Version v1.18.0-mod-rev0.4 -Clean
   ```
   Confirm:
   - All 5 roles compile and merge cleanly (`companion_radio_ble`, `companion_radio_usb`, `repeater`, `room_server`, `kiss_modem`).
   - Host path leak scan reports `0` matches across every generated binary.
   - `release/SHA256SUMS.txt` is regenerated with matching checksums.
2. **Hardware Verification (Physical Board):**
   - Flash Companion BLE (`flash.ps1 -Role ble`) and verify E-Paper boot/refresh, AHT20 reading, battery ADC curve, BLE pairing, and `RF HEALTH` view.
   - Flash Repeater / Room Server / Companion USB / KISS Modem as applicable and verify USB CDC CLI responses, microSD `sd status`, and deep sleep / WFI stability.
3. **Update Documentation & Flashers:**
   - Update fallback version filename in `release/flash.ps1` and `release/flash.bat`.
   - Update version references and binary sizes in `README.md` and `release/README.md`.
   - Add a structured functional changelog entry in `RELEASE_NOTES.md` and `docs/releases/<tag>.md` (functional changes only; do not embed internal hashes, task IDs, or host paths).
4. **Publish Tag & Automated GitHub Release:**
   - Push commits to `main` and `nm-epd-420-bw-mod`.
   - Create an **annotated** tag and push it:
     ```bash
     git tag -a v1.18.0-mod-rev0.4 -m "RockBase NM-EPD-420 MOD v1.18.0-mod-rev0.4"
     git push origin v1.18.0-mod-rev0.4
     ```
   - `.github/workflows/nm-epd-420-build.yml` automatically compiles all 5 unified binaries, verifies cleanliness, generates `SHA256SUMS.txt`, and publishes/updates the GitHub Release using `docs/releases/<tag>.md` (or the corresponding section of `RELEASE_NOTES.md`). For an already-pushed tag, you can also trigger `workflow_dispatch` manually in GitHub Actions with `release_tag` set.

# AncesTaleextended 0.1.3b (candidate)

A Qt 6 / C++ desktop application for private genealogical research and local family-group SQLite entry. It displays `missing_family_records.json`, lets the operator review/select a person and append family group rows to `family_groups.db` using the existing schema, previews Wikimedia Commons audio metadata, downloads reviewed recordings, and captures screenshots for separate approval. Optional private Eventorix command-line integration schedules a publication review reminder; it does **not** publish anything.

**This ZIP is a candidate for review, not evidence of a GitHub release or store publication.** Source is proposed under the MIT-0 license in `LICENSE`. Third-party Qt/Python dependencies and downloaded recordings are not relicensed.

## Build

Install a C++17 compiler, CMake >= 3.21, Qt 6 Widgets and Qt 6 Sql / SQLite driver, and Python 3.

```sh
cmake -S . -B build
cmake --build build --parallel 2
./build/AncesTaleextended
```

The application expects a locally configured project/data directory with a compatible private `family_groups.db`, `missing_family_records.json`, and optionally `continued_research.py` / `CONTINUED-RESEARCH.md`; those are **not included** here. Set `ANCESTALEEXTENDED_ROOT` to a directory containing your own test copies of these inputs. That setting disables optional publication scheduling. Do not run the application against another person's private data.

## Data and installation safety

Do not add actual databases, `missing_family_records.json`, audio, screenshots, account credentials, backup files, or machine-specific settings to a public repository. Review/replace local paths before distribution; this candidate still contains private-edition conventions and is **not a portable Snap/Flatpak release**.

## Review process

Read `RELEASE-REVIEW.md` before staging, committing, or publishing. A representative screenshot can be added only after explicit image approval and exact-hash checks. GitHub, Snap Store, and Flathub acceptance are distinct, unverified milestones.

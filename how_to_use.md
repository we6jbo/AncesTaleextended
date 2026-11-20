# AncesTaleextended 0.1.3
Author attribution: Jeremiah O'Neal. Project license not specified; do not assume redistribution permission.

## Local layout
- `/home/we6jbo/Projects/AncesTaleextended/main.cpp`: Qt application entry.
- `/home/we6jbo/Projects/AncesTaleextended/mainwindow.cpp`, `mainwindow.h`, `mainwindow.ui`: window, research list, family form, music and screenshot controls.
- `/home/we6jbo/Projects/AncesTaleextended/CMakeLists.txt`: Qt 6 Widgets and Sql build.
- `/home/we6jbo/Projects/AncesTaleextended/missing_family_records.json`: private input; read-only in native interface, automatically refreshed every second. Existing `continued_research.py` and `CONTINUED-RESEARCH.md` remain untouched. Open existing research editor to add/resolve JSON records.
- `/home/we6jbo/Projects/AncesTaleextended/family_groups.db`: existing SQLite database, never replaced by installer. 45 data columns (five roles × nine fields) plus primary key. `record_notes` stores provenance. No schema changes. Blank values stored as NULL.
- `/home/we6jbo/Projects/AncesTaleextended/Music/`: each imported audio file and metadata.json in its own unique recording directory. Metadata includes original page, direct URL, artist, credit, license, license URL and retrieval timestamp. No publication.
- `/home/we6jbo/Projects/AncesTaleextended/music_import.py`: local Commons metadata and download helper.
- `/home/we6jbo/Projects/AncesTaleextended/.local-logs/session.log`: local milestone log.
- `/home/we6jbo/Projects/AncesTaleextended/.install-backups/`: prior versions of replaced source files and install checkpoint logs.
- `/home/we6jbo/.38bTqEvQc/ancestaleextended/`: unique local screenshots; external registry `/home/we6jbo/.38bTqEvQc/infoartrecorder.db` is managed only by infoartrecorder.

## Use
Open CMakeLists.txt in Qt Creator, configure Qt 6 and build. Requires Qt 6 Widgets, Sql (QSQLITE driver), Python 3, CMake and a compiler. Set ANCESTALEEXTENDED_ROOT to a directory containing copies of the two inputs for test-only operation. Screenshot capture always uses the user's screenshot directory; avoid clicking it during a data-only test.

Select a person under Continued Research, inspect the raw record (including unverified recheck notes), click Use selected person, and review the editable name split, dates, and parent suggestions. Spouse and child fields are deliberately blank: the input may list multiple or conflicting relationships. Add one reviewed spouse and/or child per group; repeat for additional children or spouses, without implying relationships that evidence does not establish. Every Save appends a new row and provenance note in a single SQLite transaction; it does not update existing rows, deduplicate, or mark JSON resolved. The archived database_record_id is not a family_groups primary key. Review before saving; avoid duplicate submissions. Use the existing Python research editor for JSON changes. The native list never writes JSON.

Music: paste a Commons File URL, read metadata, review rights for the recording and composition, then confirm local download. The helper rejects unrecognized licenses and non-audio files. It does not establish legal eligibility independently. Retain metadata; do not publish recordings or private research data. Network failures leave no completed import.

Screenshot: use bottom-right button. Captures the real application widgets without a dialog, saves a uniquely named PNG, then calls infoartrecorder add with an absolute path and project name. Verify ownership and pending review with `infoartrecorder list --project AncesTaleextended`. No automated approval or publication. Existing registry defaults must be independently verified. Screenshots can include private names; review before any export.

## Local review and next steps
Check `command -v J03Curator`, `J03Curator status`, `J03Curator list`, and read `/home/we6jbo/.jchjtzir/manual.md`. Register only if absent using `J03Curator add AncesTaleextended /home/we6jbo/Projects/AncesTaleextended/main.cpp`, then `J03Curator handoff AncesTaleextended`. Never reset existing classifications, approve images, export or publish without separate approval. GitHub, Snap and blog publication are not part of this installer. Test with copies before changing the real database; close running app before installing.

## 0.1.3 fix
Commons metadata may contain numeric, null or structured values. Display these safely while retaining original raw metadata. License restrictions are unchanged.

## Optional Eventorix publication review (private edition)
On launch, a detached Python helper runs once, unless ANCESTALEEXTENDED_DISABLE_PUBLICATION_REVIEW or ANCESTALEEXTENDED_ROOT is set. It uses the native executable in ~/.local/bin/eventorix only, no SQLite access. Status files are in ~/.tvv-nov19; all five JSON files are project-tagged; if another project owns them the helper stops rather than overwrite. The history is append-only. Exact scheduling is checked against returned due_at; absolute and relative-second syntax are attempted but not confirmed on this machine. Failure is logged, not treated as success. Review the status after launch. Only due, unfinished tasks are moved 14 days; a stored unfinished scheduling attempt can be corrected on relaunch. No automatic publishing.

For AI agents: read AI-STATUS.md and all publication JSON files before work. In publication-status.json, configure approval only after explicit owner approval: {"owner_approved":true,"license":"MIT-0","targets":{"github":{"repo":"OWNER/REPO","commit":"40-character SHA","license_path":"LICENSE","ai_instructions_path":"AI-REUSE.md","approved_sha256":{"LICENSE":"sha256","AI-REUSE.md":"sha256","main.cpp":"sha256"}},"snap":{"name":"approved-snap-name","publisher":"approved-publisher-username","version":"approved-version"},"flatpak":{"channel":"flathub","app_id":"approved.app.ID","version":"approved-version"}}}. This is the value of the approval field, not the whole file. Include every approved source file in approved_sha256. No target is assumed by default. Live verification checks public GitHub commit and approved hashes, stable Snap channel and publisher, and Flathub appstream identity/release. Other Flatpak channels require a new verification adapter. API schema failures are unverified, previous evidence preserved. No automatic license change. Review private paths and data before any public export; this ZIP is a private development package, not approved public source.

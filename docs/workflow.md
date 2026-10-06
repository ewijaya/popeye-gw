# Popeye G&W workflow: the plain-English guide

How to work on, back up and release the **game app** and the **watchface**,
explained step by step. Written 2026-10-06. Where something is planned but not
built yet, it says so.

---

## 1. The big picture in one minute

- There is **one project** on GitHub: <https://github.com/ewijaya/popeye-gw>.
- That project contains **two products**:

  | Product | What it is | Lives in | Released as |
  |---|---|---|---|
  | **Popeye G&W** (the game app) | the game, clock and alarm | `src/`, `resources/` | tags `v1.0.2`, `v1.1.0`, … |
  | **Popeye G&W Clock** (the watchface) | an animated clock face | `watchface/` | tags `clock-v1.0.0`, … |

- The two products **share some code and art**: `src/c/scene.c`, `clock.c`,
  `game.c` and the pictures in `resources/images/`. The watchface borrows them
  from the game rather than keeping copies.
- Each product has its **own version number, changelog, PBW file and store
  listing**. You can release one without releasing the other.

---

## 2. Words you need (mini dictionary)

| Word | Plain meaning |
|---|---|
| **Repo** (repository) | The project and its complete history. One copy is on GitHub; one copy is on your Mac in `~/Desktop/popeye-gw/.git`. |
| **Commit** | A saved snapshot of your changes, **on your Mac only**. |
| **Push** | Upload your commits to GitHub. Only now can other people see them, because the repo is public. |
| **Branch** | A separate line of work, like a draft. `main` is the official version; `feat/...` branches are drafts. |
| **`main`** | The official branch. It always matches what has been released. |
| **Merge** | Copy the finished work from a draft branch into `main`. |
| **Worktree** | An extra folder on your Mac showing a different branch of the **same** repo. It lets you work on two things at once without them getting in each other's way. |
| **PBW** | The installable watch file, like an app installer. |
| **Release** | Publish a PBW to GitHub Releases and the RePebble store, after you approve it. |
| **Tag** | A permanent label on the exact commit that was released, e.g. `v1.0.2`. |
| **CI** | GitHub's robot that runs the tests on every push. |

---

## 3. Your two folders

```
~/Desktop/popeye-gw/              ← MAIN FOLDER (holds the real repo database)
   .git/   (a real folder: all history, all branches, link to GitHub)
   working on branch: feat/v1.1   (game v1.1 features, not released)

~/Desktop/popeye-gw-watchface/    ← WATCHFACE FOLDER (a worktree)
   .git    (just a tiny FILE pointing to ~/Desktop/popeye-gw/.git)
   working on branch: feat/watchface   (watchface, ready to polish and release)
```

Key facts:

1. **Same repo, same GitHub.** Both folders push to
   `github.com/ewijaya/popeye-gw`. There is no second repo.
2. **Different branches.** Each folder shows its own branch, so edits in one
   folder do not appear in the other until they are merged.
3. **Commits are shared instantly.** A commit made in either folder is visible
   to git in the other folder, e.g. `git log feat/watchface`. Uncommitted edits
   are not.
4. **Don't delete or move `~/Desktop/popeye-gw`.** The watchface folder depends
   on it. Don't move the watchface folder by hand either; use git (section 9).
5. **Only one folder can have `main` open at a time.** Git enforces this.
6. **The watchface folder itself is never uploaded.** Only commits you push go
   to GitHub.

To see all folders: `git worktree list`. You'll also see
`.release/1.0.2-audit/source`, a leftover from the 1.0.2 release check. Ignore it.

---

## 4. Choosing what to work on

You choose by **which folder you start Claude in**:

```sh
# Work on the GAME
cd ~/Desktop/popeye-gw && claude

# Work on the WATCHFACE
cd ~/Desktop/popeye-gw-watchface && claude
```

- `claude --continue` resumes the last chat **from that folder**, and
  `claude --resume` lists chats **from that folder**. So game chats and
  watchface chats stay separate.
- The git status at the top of each chat shows the branch. Check it: game work
  is `feat/v1.1`, watchface work is `feat/watchface`.
- The first time in the watchface folder, say:
  **"Read HANDOFF-watchface.md and continue."**

**Can Claude in one folder touch the other folder?** Only on purpose.
- Files outside the folder you started in need your permission.
- Git is shared, so Claude can read the other branch's committed work.
- Each session is told to stay in its own folder. If you want this strictly
  enforced, ask Claude to add a "deny" permission rule.

---

## 5. Everyday work and backing up (`/gp`)

The everyday loop, in either folder:

1. Ask Claude for a change.
2. Claude runs the tests (`./tools/test.sh`). These always test **both**
   products because they share code.
3. Commit, which saves on your Mac.
4. Optionally back up to GitHub with `/gp`.

### What `/gp` really does
1. `git add -A`: picks up **every** changed or new file in the folder.
2. Commits with an automatic message.
3. `git push origin HEAD`: uploads **the current branch**, under the same name.

So:

| You run `/gp` in… | It uploads to GitHub branch… | Changes `main` or releases? |
|---|---|---|
| `~/Desktop/popeye-gw` | `feat/v1.1` | **No** |
| `~/Desktop/popeye-gw-watchface` | `feat/watchface` | **No** |

You can see a pushed branch at
`https://github.com/ewijaya/popeye-gw/tree/<branch-name>` (pick it in the branch
dropdown). The repo's front page shows `main` and does not change.

### `/gp` warnings
- **It uploads everything in the folder.** Keep scratch notes elsewhere, or in
  ignored files. The repo is public.
- **Known trap in the main folder:** `PRD.md` currently shows as deleted there.
  `/gp` would delete it on GitHub too. Fix that first: run `git restore PRD.md`
  if the deletion wasn't on purpose.
- **It can hide a failed upload.** It always says "Done!". Check with
  `git status -sb`: if it says `ahead`, the push did not happen.
- **Every push runs CI** (the test robot). See the **Actions** tab on GitHub.

---

## 6. Releasing: the rules that never change

A release always follows the same safety steps, for either product:

1. The work is finished and **merged into `main`**. `main` is pushed and CI is green.
2. Claude **builds and checks** it: tests, size limits, identity.
3. Claude **freezes** one PBW file. It is never rebuilt after this point.
4. **You install that exact file on your watch and play-test it.**
5. **You approve** that exact file by its SHA-256 fingerprint, version and
   destinations. Nothing is published without this.
6. Claude publishes it to GitHub Releases and the RePebble store, then verifies
   both.
7. Claude records the result in the README.

The details live in `docs/releasing.md` and the three release skills
(`popeye-gw-build-audit`, `popeye-gw-release`, `popeye-gw-appstore`).

### Two important "gotchas"
- **You release from `main`, not from a draft branch.** So you merge first,
  then release, in whichever folder has `main` open. Only one folder can.
- **The release skills currently only know the game.** They have the game's ID
  and file name built in and would refuse the watchface. *Planned:* the
  watchface session will upgrade them to handle both, with the app as a
  parameter (`popeye-gw` or `popeye-gw-clock`).

  Skills are files on a branch, so a folder sees the upgraded skills only once
  its branch contains that upgrade:

  | When | Watchface folder | Game folder (`feat/v1.1`) |
  |---|---|---|
  | After the upgrade is committed on `feat/watchface` | upgraded skills | old, game-only skills |
  | After it's merged to `main` and `main` is merged into `feat/v1.1` | upgraded skills | upgraded skills |

---

## 7. How to release ONLY the watchface

Stay in the watchface folder. The game is not touched and stays at v1.0.2.

```sh
cd ~/Desktop/popeye-gw-watchface && claude
```
Then ask Claude to release the watchface. Claude will:

1. Finish the watchface work and the skills upgrade on `feat/watchface`.
2. Ask you, then switch **this folder** to `main`, merge `feat/watchface` and
   push. This is allowed because the game folder is on `feat/v1.1`, not `main`.
3. Freeze `popeye-gw-clock.pbw`. **You play-test it on the watch and approve it.**
4. Register a **new** store listing for the watchface (first time only), then
   create the GitHub release `clock-v1.0.0`, marked as **not** "Latest" so the
   game stays the headline download.
5. Verify both, then record the result.

Afterwards, **give `main` back** (section 9) so the game folder can use it.

---

## 8. How to release the game (e.g. v1.1)

Do this **after** the watchface folder has let go of `main`.

```sh
cd ~/Desktop/popeye-gw
git status          # must be clean: settle the PRD.md deletion first
git switch main
git pull            # gets the watchface release and the upgraded skills
```
Then ask Claude to release the game. It merges `feat/v1.1`, picks the version
(likely 1.1.0) and goes through section 6.

v1.1 notes:
- **Sound** can only be judged on the real watch, by ear.
- **Online leaderboard:** v1.1 can ship with the **Online** setting off and no
  server address. Running the leaderboard server is a separate, later decision.

---

## 9. Giving `main` back, or removing the watchface folder

Close any Claude chat in the watchface folder first.

### Option A: keep the watchface folder for future work
```sh
cd ~/Desktop/popeye-gw-watchface
git status                          # should be clean
git switch -c feat/watchface-next   # new draft branch; main is now free
```

### Option B: remove the watchface folder (fine when the watchface is done for now)
```sh
cd ~/Desktop/popeye-gw
git worktree remove ../popeye-gw-watchface   # deletes the folder only
git branch -d feat/watchface                 # optional: it's already in main
git worktree list                            # check it's gone
```
- Git **refuses** if there are unsaved changes. That's protection. Don't use
  `--force` unless you're sure.
- Lost: only throwaway files (`watchface/build/`, `HANDOFF-watchface.md`).
- Kept: all code, all history, every release.
- If you pushed the branch: `git push origin --delete feat/watchface` cleans
  GitHub too.

---

## 10. Coming back to the watchface later

Removing the folder loses nothing. After the release, `main` contains
`watchface/` forever, so it is in every copy of the project.

**Not doing game work at the same time?** Use the main folder:
```sh
cd ~/Desktop/popeye-gw
git switch main && git pull
git switch -c feat/watchface-1.1
cd watchface && pebble build
```

**Game work in progress at the same time?** Recreate the extra folder:
```sh
cd ~/Desktop/popeye-gw
git worktree add ../popeye-gw-watchface -b feat/watchface-1.1 main
cd ../popeye-gw-watchface && claude
```

Rule of thumb: **a worktree is a disposable desk.** Set one up when you need
two jobs going at once; clear it away when you're done.

---

## 11. The one thing that connects both products: shared code

| If you change… | It affects… |
|---|---|
| `watchface/` only | the watchface only |
| game-only files (`src/c/main.c`, `view.c`, menus, sound, …) | the game only |
| **shared** files (`src/c/scene.c`, `clock.c`, `game.c`, `resources/images/`) | **both** products' next builds |

- Already-released PBWs never change by themselves. Users see nothing until you
  release again.
- After a shared change, ask: "Does the other product need a new release too?"
  Art or clock fixes usually yes; game-rule changes usually no.
- The tests always cover both, so a shared change that breaks the watchface is
  caught immediately.

---

## 12. Standing rules (always true)

- **Nothing is published without your explicit approval** of the exact file,
  after you've play-tested it on your watch.
- **Nothing is pushed unless you ask** (`/gp`, or "push this").
- **`PRD.md` is only edited with your say-so.** Decisions made along the way go
  in `docs/` (e.g. `docs/v1.1-notes.md`).
- Commit messages are short and conventional (`feat:`, `fix:`, `docs:` …) with
  **no AI attribution**.
- There is **one watch emulator**. Two chats shouldn't use it at the same time.
- Never delete or move `~/Desktop/popeye-gw` while a worktree exists.

---

## 13. The plan from here (as of 2026-10-06)

1. **Game folder (`feat/v1.1`):** finish the v1.1 features: leaderboard, docs
   and GIF, then a final check. Not pushed, not released.
2. **Watchface folder (`feat/watchface`):** play-test, upgrade the release
   skills for two apps, prepare store material, release **`clock-v1.0.0`**.
3. **Give `main` back** (section 9).
4. **Game folder:** pull `main`, play-test v1.1 on the watch, release **v1.1.0**.
5. **Remove the watchface worktree** when you no longer need two jobs at once.

---

## 14. Cheat sheet

| I want to… | Do this |
|---|---|
| Work on the game | `cd ~/Desktop/popeye-gw && claude` |
| Work on the watchface | `cd ~/Desktop/popeye-gw-watchface && claude` |
| See which branch I'm on | `git branch --show-current` |
| See all my folders | `git worktree list` |
| Back up my branch to GitHub | `/gp` (read section 5 warnings) |
| Check the backup really happened | `git status -sb` (no `ahead` = good) |
| Release the watchface | in the watchface folder: "release the watchface" |
| Release the game | free up `main` first, then in the game folder: "release the game" |
| Free up `main` | section 9, option A or B |
| Restart watchface work later | section 10 |
| Get help | just ask Claude in plain words, e.g. "free up main" |

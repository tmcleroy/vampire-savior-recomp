**What this changes and why**

**Checklist**
- [ ] Nothing from the game (files, save states, generated code, dumps, screenshots), no keys or key values, nothing from Capcom's own materials or a leak.
- [ ] `make audit` passes.
- [ ] `make check` passes over the ladder movie (with every feature off the game runs as before), if the translator, the machine or game code changed; `make check-rewrites` if a rewrite did.
- [ ] Gameplay and control changes are an optional feature, off in Pure.
- [ ] No added input lag or lost speed (say what was measured, if timing, input, sound or presentation changed).
- [ ] Keyboard and gamepad both still work.
- [ ] `cps2recomp/` and `cps2recomp/recomp-common/` are unchanged (they are copies).
- [ ] A line in `CHANGELOG.md` under "Unreleased"; docs updated if behavior changed.

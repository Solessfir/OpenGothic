# Local patches of the submodules

`lib/ZenKit` is the upstream GothicKit/ZenKit, OpenGothic needs three changes that are not in it yet:

- `DaedalusVm::invoke_function` - a call of a function with the arguments already on the stack (`AI_Function`, `debug.call`);
- `DaedalusVm::pop_int` accepts an instance and returns the index of its symbol, as the original engine does (the destructor
  `Bar_Delete(var int)` of LeGo is called with an instance);
- `INpc::compat_focus_vob` / `compat_interact_mob` - the members of oCNpc that C_NPC does not have.

CMake applies the patch by itself (`cmake/zenkitpatch.cmake`, before ZenKit is added): a clean clone, the Android build and the CI need nothing more. A tree that has it already is left alone. By hand (the patch fails when it was applied already):

```bash
git -C lib/ZenKit apply --ignore-whitespace ../../patches/zenkit-opengothic.patch
```

Make the patch again after a change in the submodule: `git -C lib/ZenKit diff > patches/zenkit-opengothic.patch`.

# noexcept policy (M0)

- Hook dispatch in `core/hooks.hpp` is `noexcept` by contract.
- Core trampoline paths that only dispatch hooks and/or mutate POD state are marked `noexcept` where practical (`arrange`, `manage`, `resize`, `run`, `setup`, `tile`, focus command adapters).
- Xlib-facing functions are not globally forced to `noexcept` in M0 to avoid silently hard-terminating on unexpected exceptions from future extension code.
- Rule for mods: hook implementations must be `noexcept`.

## Summary

<!-- Describe what this PR changes and why. -->

## Type of change

- [ ] Bug fix
- [ ] New feature / new layer implementation
- [ ] Refactor (no behavior change)
- [ ] Documentation
- [ ] CI / tooling

## Hardware tested

<!-- Check every configuration you physically tested on device. -->
- [ ] Tested on Xteink X4 hardware
- [ ] Tested in PlatformIO simulation / host build only
- [ ] Not applicable (docs/CI only)

## Checklist

- [ ] `pio run -e dev` builds without errors
- [ ] `pio run -e release` builds without errors
- [ ] All `[X4:...]` serial markers appear as expected on device
- [ ] No Wi-Fi credentials, tokens, or secrets committed
- [ ] `CONFIG_X4_*` flags default to `0` in `[env:release]`
- [ ] Display changes tested with full refresh + checkerboard pattern
- [ ] OTA rollback not broken (if OTA code changed)
- [ ] Documentation updated if public API changed

## Related issues

<!-- Closes #XX -->

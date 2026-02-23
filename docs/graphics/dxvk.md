# DXVK runtime profile (`dxvk.conf`)

When `enableDxvk=1` is set in `options.cfg`, setup/deployment now writes a `dxvk.conf` file in the same directory as `SwgClient_r.exe` (the local client executable directory).

## Location and lifecycle

- Setup flow (`SwgClientSetup` save): generates `dxvk.conf` beside the executable whenever DXVK is enabled.
- Setup flow with DXVK disabled: removes any previously-generated `dxvk.conf` from that same location.
- Packaging/deployment flow (`tools/win32/package-win32-renderers.ps1 -Runtime dxvk`): copies `d3d9.dll` and generates `dxvk.conf` into each output target.
- Packaging/deployment with `-Runtime native`: removes both `d3d9.dll` and `dxvk.conf` overrides from targets.

## Profile behavior

The generated profile chooses defaults by detected adapter identity:

- AMD (`VendorId=0x1002` with a non-zero `DeviceId`): AMD conservative profile for D3D9 pacing.
- Non-AMD: generic conservative profile.

Current conservative defaults include:

- `d3d9.presentInterval = 1`
- `d3d9.maxFrameLatency = 1` (AMD) or `2` (generic)
- `dxvk.enableAsync = False`
- AMD profile additionally sets `dxgi.maxFrameLatency = 1`

## Overrides

Operators can override the generated config by either:

1. Editing `dxvk.conf` in-place next to the client executable, or
2. Setting `DXVK_CONFIG_FILE=<path>` to point DXVK at an alternate config file.

## Troubleshooting

- Verify DXVK is active by checking that `d3d9.dll` (DXVK runtime) exists beside `SwgClient_r.exe`.
- Verify profile generation by checking for `dxvk.conf` in the same folder.
- If behavior is worse after tuning:
  - Temporarily remove/rename `dxvk.conf` to test DXVK defaults.
  - Set `dxvk.enableAsync = True` only as an experiment and validate content correctness.
  - Increase `d3d9.maxFrameLatency` cautiously (for example from `1` to `2`) if pacing appears too strict.
- If you must force an emergency profile without touching client directory files, set `DXVK_CONFIG_FILE` to a known-good profile path.

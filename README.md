# Intel VA-API driver for legacy hardware.

This is a fork with many changes, including but not limited to:

- Functioning Chromium support [✻]
- Fix JPEG decoding of oddly encoded files on gen7/8 (https://github.com/intel/intel-vaapi-driver/pull/514)
- fix exporting buffers with 3 planes and `VA_EXPORT_SURFACE_SEPARATE_LAYERS` (https://github.com/intel/intel-vaapi-driver/pull/530)
- `i965_pci_ids`: Add CFL PCI ID found on Xeon W-1290P (https://github.com/intel/intel-vaapi-driver/pull/548)
- Make wl_drm optional (https://github.com/intel/intel-vaapi-driver/pull/566)
- Expose `VAConfigAttribMaxPictureWidth` and `VAConfigAttribMaxPictureHeight`.
- Full support for `vaSyncBuffer`, `vaSyncSurface2` and `vaMapBuffer2`. (enabling `async_depth` in FFMPEG)
- More accurate per-codec limits.
- Improved hybrid codec driver support.
- Expose ARGB format support. (https://github.com/intel/intel-vaapi-driver/issues/500)

The `NEWS` file contains a more detailed changelog.

[✻] Works on IVB and newer, semi-broken on SNB and ILK, unsupported on CTG

# Release schedule

This driver has a single digit count of planned releases, after that this driver will too be strictly
in maintenance mode, since it's basically feature complete.

Until maintenance mode is reached, the driver will continue to increment on the `2.4.x` version branch,
and then bump the version to `2.5.x` where only fixes for regressions and critical issues will be accepted.

# Reporting issues

- Reproducers are highly welcome (i don't like seeing "it doesn't work")
- Sample footage (if possible) for any fork regressions are welcome
- Remember that I am just some random person on the Internet.

# GPU support matrix

| GPU Generation | MPEG-2              | VC1    | H.264 (AVC)         | H.265 (HEVC)        | JPEG                | VP8                 | VP9                    |
|----------------|---------------------|--------|---------------------|---------------------|---------------------|---------------------|------------------------|
| G45 (CTG)      | DECODE              |        |                     |                     |                     |                     |                        |
| ILK (5.x)      | DECODE              |        | DECODE              |                     |                     |                     |                        |
| SNB (6.x)      | DECODE              | DECODE | DECODE<br>ENCODE[1] |                     |                     |                     |                        |
| IVB (7.0)      | DECODE<br>ENCODE[1] | DECODE | DECODE<br>ENCODE[1] |                     | DECODE              |                     |                        |
| HSW (7.5)      | DECODE<br>ENCODE[1] | DECODE | DECODE<br>ENCODE[1] |                     | DECODE              |                     | DECODE[2]              |
| CHV (8.0 LP)   | DECODE<br>ENCODE[1] | DECODE | DECODE<br>ENCODE[1] | DECODE              | DECODE<br>ENCODE[1] | DECODE<br>ENCODE[1] | DECODE[2]              |
| BDW (8.0) [3]  | DECODE<br>ENCODE[1] | DECODE | DECODE<br>ENCODE[1] |                     | DECODE              | DECODE              | DECODE[2]              |
| SKL (9.0) [3]  | DECODE<br>ENCODE[1] | DECODE | DECODE<br>ENCODE[4] | DECODE<br>ENCODE[1] | DECODE<br>ENCODE[1] | DECODE<br>ENCODE[1] | DECODE[2]              |
| KBL (9.5) [3]  | DECODE<br>ENCODE[1] | DECODE | DECODE<br>ENCODE[4] | DECODE<br>ENCODE[1] | DECODE<br>ENCODE[1] | DECODE<br>ENCODE[1] | DECODE[5]<br>ENCODE[6] |
| ??? (10.0)     | ?                   | ?      | ?                   | ?                   | ?                   | ?                   | ?                      |

[1] - Intel's hardware encoder uses GPU resources during an encode, high GPU load can cause bad encoding performance.

[2] - "Hybrid" support only, known to be buggy.

[3] - Supported by the iHD driver, **you shouldn't be using this driver**

[4] - Supports Low Power encoders, which fully offload the work without using GPU resources.

[5] - Supports VP9 natively.

[6] - Encoding is supported by this driver, known to be buggy. Likely safe up to 1920x1080 at 60 FPS.

# License.

```
MIT License

Copyright (c) 2023 Intel Corporation

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
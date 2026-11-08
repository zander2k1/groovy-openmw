# GroovyMiSTer library

Used by Groovy OpenMW to stream the game to a MiSTer FPGA running the
[Groovy_MiSTer](https://github.com/psakhis/Groovy_MiSTer) core.

- `groovymister*.cpp/.h`, `rio.h`: GroovyMiSTer client library by psakhis (Sergi Clara), as
  included in [Groovy Dolphin](https://github.com/zander2k01/groovy-dolphin), with `gmw_reset()`
  added for reconnecting.
- `lz4*.c/.h`: [LZ4](https://github.com/lz4/lz4) v1.10.0 by Yann Collet, BSD 2-clause
  (see `LICENSE.lz4`).

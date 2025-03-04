![ngPlant Logo](ngplant/images/ngplant-128x128.png)
# ngPlant version 0.9.14

`INSTALL` file contains instructions on how to build and install **ngPlant** libraries and applications.

The user manual is, for now, being kept up-to-date on [this project's GitHub wiki](../../wiki/). It also includes more detailed installation notes.

********************

## Directory structure description

- **ngpcore**     - `libngpcore` sources
- **ngput**       - `libngput` sources
- **pywrapper**   - Python bindings sources (`_ngp` module)
- **ngplant**     - `ngplant` application sources
- **ngpshot**     - `ngpshot` application sources
- **ngpview**     - example application, which uses `ngpcore` high-level interface to render plant models
- **docapi**      - ngpcore high-level programming interface documentation for C++ and Python (xml sources and generated `.html` documentation)
- **config**      - local configuration files
- **sctool**      - helper modules for `SCons`
- **devtools**    - additional tools
- **samples**     - sample plant models and textures
- **scripts**     - sample Python scripts
- **plugins**     - ngplant plug-ins directory
- **extern/lua**  - Lua 5.1.4 source code. Please read `README.extern` file for more detailed information.
- **extern/glew** - GLEW 1.4.0 source code. Please read `README.extern` file for more detailed information.

`ngplant/images/ngplant.svg` is ngplant's icon image in `svg` format. It was created by Yorik van Havre.

## License

`ngpcore`, `ngput` and `pywrapper` libraries are distributed under the terms of the **BSD License** - see `COPYING.BSD` file.

`ngplant`, `ngpshot` and `ngpview` applications are distributed under the terms of the **GNU General Public License** - see `COPYING` file.

For information about Lua source code license please read `README.extern` and `extern/lua/COPYRIGHT`.

For information about GLEW source code license(s) please read `README.extern` and `extern/glew/CREDITS`.


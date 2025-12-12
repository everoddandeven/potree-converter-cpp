# Potree Converter Cpp
A C++ library for easy Potree point cloud conversion.

  - This project is based on [PotreeConverter 2.0](https://github.com/potree/PotreeConverter).
  - PotreeConverter is an octree LOD generator for streaming and real-time rendering of massive point clouds created by Markus Schütz.
  - About 10 to 50 times faster than PotreeConverter 1.7 on SSDs.
  - Produces a total of 3 files instead of thousands to tens of millions of files. The reduction of the number of files improves file system operations such as copy, delete and upload to servers from hours and days to seconds and minutes. 
  - Better support for standard LAS attributes and arbitrary extra attributes. Full support (e.g. int64 and uint64) in development.

This project represents an effort to make the structure of the original [PotreeConverter](https://github.com/potree/PotreeConverter) and its code more modular, in order to facilitate its maintenance and integration into system services.

# Build instructions

1. Clone this repository

    ```bash
    git clone --recursive https://github.com/everoddandeven/potree-converter-cpp.git
    ```

2. Build potree-converter-cpp

    ```bash
    mkdir build
    cd build
    cmake ..
    cmake --build .
    ```


# Publications by Markus Schütz.

* [Potree: Rendering Large Point Clouds in Web Browsers](https://www.cg.tuwien.ac.at/research/publications/2016/SCHUETZ-2016-POT/SCHUETZ-2016-POT-thesis.pdf).
* [Fast Out-of-Core Octree Generation for Massive Point Clouds](https://www.cg.tuwien.ac.at/research/publications/2020/SCHUETZ-2020-MPC/), _Schütz M., Ohrhallinger S., Wimmer M._


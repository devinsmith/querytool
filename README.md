# querytool
A simple GUI based SQL query tool

# Dependencies

The application is dependent on a fairly obscure UI toolkit called FOX.
However, it is likely packaged for your distribution. See
[repology](https://repology.org/project/fox-toolkit/versions) for details.

On Debian based distros you can install the dependencies with apt-get:

```
sudo apt-get install libfox-1.6-dev
```

# Building

Use CMake

```
mkdir build
cd build
cmake ..
make
```

# Screenshots

![Main](img/querytool_001.png)

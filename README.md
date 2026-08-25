
![image](./media/logo.gif)

TUI Audio Visualizer built for the Pipewire multimedia server

![image](./media/showcase.gif)

# Installation
```
git clone https://github.com/WidRojas/Audio_Visualiser.git
cd Audio_Visualizer
make
```
# Quickstart

1. Run Indicate
2. Pipe desired audio channel into sink via CLI tools or external software(qpwgraph)

``` bash

# application arguments :

./Indicate (int: bars) (double: sensitivity) (int: character limit) ("enable"/"disable") (serial location: ex:"/dev/ttyUSB0")
```

when sending UART data, format is sent as...

```

[start bit]  bars * [bar data (0 -100)] [end bit]

so when set to 3 bars the packet would look like

[start bit, 10 , 25 ,75 , endbit]

```

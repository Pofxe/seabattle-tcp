# seabattle-tcp

## Install ##


#### Compiling

```bash
git clone https://github.com/Pofxe/seabattle-tcp.git
cd seabattle-tcp
mkdir build
cd build
conan install ..
cmake ..
cmake --build .
```

## Requirements ##

- [boost](https://www.boost.org/)

## Usage ##

```bash
seabattle <sid> <port> - для запуска сервера
seabattle <sid> <IPv4> <port> - для запуска клиента(порт тот же, что и у сервера)
```
Example
```bash
seabattle 1111 1234
seabattle 2222 127.0.0.1 1234
```

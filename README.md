# UnholyC

<img align="right" src="./logo.png" width=10%>

UnholyC is a custom dialect transpiled to C++

https://www.uhclang.org/

## Build
Run
```bash
./build-all.sh
```

Then you will have all files inside dist for the transpiler, std lib and graphic lib

## Usage
```bash
unholyc <input_dir> <output_dir> [-I<include_dir> ...]
```

```
unholyc ./ out/ -Iuchstd
```

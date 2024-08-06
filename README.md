# AO-Llama

## USAGE

### Build Libs

First build llama cpp and ao-llama ( the bindings )
```sh
./build_libs.sh
```

Next build the wasm ( this also copies the process.js and process.wasm to the test-llm dir )
```sh
./build_wasm.sh
```

### Testing

```sh
cd test-llm
yarn # or npm i
yarn test # or npm run test
```
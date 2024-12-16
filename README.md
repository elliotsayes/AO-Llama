# AO-Llama

## Usage

### 1. Pull Submodules

To pull the latest version of the submodules, run the following command:
```bash
git submodule update --init --recursive
```

### 2. Build

To build the necessary libraries, inject them, and compile the WebAssembly (WASM) binary, execute the following:
```bash
./build.sh
```

### 3. Testing

To run tests, navigate to the `tests` directory, install dependencies, and run the tests:
```bash
cd tests
npm install
npm run test
```

### 4. Starting a Local Server

To avoid downloading models every time you run the application, you can set up a local server to host the models from your machine. This will allow you to access the models locally, which improves efficiency.

For detailed instructions on setting up and running the server locally, refer to the `README.md` file located in the `./server` folder: [./server/README.md](./server/README.md).

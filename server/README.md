# Local Setup Instructions

To get started with testing modules locally, follow the steps below:

## 1. Download Modules

First, download the modules you want to use and place them in this folder. You can use `curl` for this:

```bash
curl -L -O https://arweave.net/M-OzkyjxWhSvWYF87p0kvmkuAEEkvOzIj4nMNoSIydc
```

Repeat this for each module you need.

## 2. Start Local Server

Next, run `start_local_server.sh` to start a local Nginx server that will host your modules.

```bash
./start_local_server.sh
```

This will start the server on `http://localhost:3001`.

## 3. Update ARWEAVE Field

Now, update the `ARWEAVE` field in the options object inside `llama.test.js` to point to your local server.

Find the following code in `llama.test.js`:

```javascript
const options = {
  format: "wasm64-unknown-emscripten-draft_2024_02_15",
  admissableList: AdmissableList,
  WeaveDrive: weaveDrive,
  ARWEAVE: 'https://arweave.net',
  mode: "test",
  blockHeight: 100,
  spawn: {
    "Scheduler": "TEST_SCHED_ADDR"
  },
  Process: {
    Id: 'AOS',
    Owner: 'FOOBAR',
    tags: [
      { name: "Extension", value: "Weave-Drive" }
    ]
  },
}
```

Update the `ARWEAVE` URL to:

```javascript
ARWEAVE: 'http://localhost:3001',
```

## 4. Update the Selected Module

Finally, update the `selectedModule` variable in `llama.test.js` with the model you downloaded.

Make sure to reference the correct file name for the downloaded module (`const selectedModel = 'M-OzkyjxWhSvWYF87p0kvmkuAEEkvOzIj4nMNoSIydc';`).


import { test } from 'node:test'
import * as assert from 'node:assert'
import AoLoader from '@permaweb/ao-loader' // Need new version of AO Loader
import fs from 'fs'
import weaveDrive from './weavedrive.js';
const AdmissableList =
  [
    "dx3GrOQPV5Mwc1c-4HTsyq0s1TNugMf7XfIKJkyVQt8", // Random NFT metadata (1.7kb of JSON)
    "XOJ8FBxa6sGLwChnxhF2L71WkKLSKq1aU5Yn5WnFLrY", // GPT-2 117M model.
    "M-OzkyjxWhSvWYF87p0kvmkuAEEkvOzIj4nMNoSIydc", // GPT-2-XL 4-bit quantized model.
    "kd34P4974oqZf2Db-hFTUiCipsU6CzbR6t-iJoQhKIo", // Phi-2 
    "ISrbGzQot05rs_HKC08O_SmkipYQnqgB1yC3mjZZeEo", // Phi-3 Mini 4k Instruct
    "sKqjvBbhqKvgzZT4ojP1FNvt4r_30cqjuIIQIr-3088", // CodeQwen 1.5 7B Chat q3
    "Pr2YVrxd7VwNdg6ekC0NXWNKXxJbfTlHhhlrKbAd1dA", // Llama3 8B Instruct q4
    "jbx-H6aq7b3BbNCHlK50Jz9L-6pz9qmldrYXMwjqQVI", // Llama3 8B Instruct q8
    "eZFVj31sV6Ou8FRWqMVEQ4r-9vQqp4B3U-u2tisg_PM", // all-MiniLM-L6-v2.Q4_0.gguf
  ]


const wasm = fs.readFileSync('./process.wasm')
const options = {
  format: "wasm64-unknown-emscripten-draft_2024_02_15",
  admissableList: AdmissableList,
  WeaveDrive: weaveDrive,
  ARWEAVE: 'https://arweave.net',
  mode: "test",
  blockHeight: 100,
  spawn: {
    "Scheduler": "TEST_SCHED_ADDR",
  },
  Module: {
    Id: 'WOOPAWOOPA',
    Owner: 'FOOBAR',
    Tags: [
      { name: 'Memory-Limit', value: '15-gb' }
    ],
  },
  Process: {
    Id: 'AOS',
    Owner: 'FOOBAR',
    tags: [
      { name: "Extension", value: "Weave-Drive" }
    ]
  },
}

test('llama', async () => {
  const handle = await AoLoader(wasm, options)
  const env = {
    Process: {
      Id: 'AOS',
      Owner: 'FOOBAR',
      Tags: [
        { name: 'Name', value: 'Thomas' }
      ]
    }
  }
  const msg = {
    Target: 'AOS',
    From: 'FOOBAR',
    Owner: 'FOOBAR',
    ['Block-Height']: "1000",
    Id: "1234xyxfoo",
    Module: "WOOPAWOOPA",
    Tags: [
      { name: 'Action', value: 'Eval' }
    ],
    Data: `
    local Llama = require(".Llama")
    io.stderr:write([[Loading model...\n]])
    local result = Llama.load("/data/eZFVj31sV6Ou8FRWqMVEQ4r-9vQqp4B3U-u2tisg_PM")
    io.stderr:write([[Loaded! Running Embed...\n]])
    local str1 = Llama.embed("Once upon a time, in a land far far away, there was a")
    io.stderr:write([[Embedded #1!\n]])
    local str2 = Llama.embed("Examples of technical debt include legacy code, insufficient testing, hard-coded values, outdated libraries, and more.")
    io.stderr:write([[Embedded #2!\n]])
    return str1 .. ' || ' .. str2
    `
  }

  // load handler
  const result = await handle(null, msg, env)

  console.log(result)

  assert.ok(true)
})

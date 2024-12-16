import { test } from 'node:test';
import * as assert from 'node:assert';
import AoLoader from '@permaweb/ao-loader';
import fs from 'fs';
import weaveDrive from './weavedrive.js';
import { performance } from 'perf_hooks';

const AdmissableList = [
  "dx3GrOQPV5Mwc1c-4HTsyq0s1TNugMf7XfIKJkyVQt8", // Random NFT metadata (1.7kb of JSON)
  "XOJ8FBxa6sGLwChnxhF2L71WkKLSKq1aU5Yn5WnFLrY", // GPT-2 117M model.
  "M-OzkyjxWhSvWYF87p0kvmkuAEEkvOzIj4nMNoSIydc", // GPT-2-XL 4-bit quantized model.
  "kd34P4974oqZf2Db-hFTUiCipsU6CzbR6t-iJoQhKIo", // Phi-2 
  "ISrbGzQot05rs_HKC08O_SmkipYQnqgB1yC3mjZZeEo", // Phi-3 Mini 4k Instruct
  "sKqjvBbhqKvgzZT4ojP1FNvt4r_30cqjuIIQIr-3088", // CodeQwen 1.5 7B Chat q3
  "Pr2YVrxd7VwNdg6ekC0NXWNKXxJbfTlHhhlrKbAd1dA", // Llama3 8B Instruct q4
  "jbx-H6aq7b3BbNCHlK50Jz9L-6pz9qmldrYXMwjqQVI"  // Llama3 8B Instruct q8
];

const ModuleEnum = {
  RANDOM_NFT_METADATA: AdmissableList[0],
  GPT2_117M_MODEL: AdmissableList[1],
  GPT2_XL_4BIT_MODEL: AdmissableList[2],
  PHI_2: AdmissableList[3],
  PHI_3_MINI_4K_INSTRUCT: AdmissableList[4],
  CODEQWEN_1_5_7B_CHAT_Q3: AdmissableList[5],
  LLAMA3_8B_INSTRUCT_Q4: AdmissableList[6],
  LLAMA3_8B_INSTRUCT_Q8: AdmissableList[7]
};

// Global selectedModule for the test
const selectedModule = ModuleEnum.GPT2_XL_4BIT_MODEL;

const wasm = fs.readFileSync('./process.wasm');
const options = {
  format: "wasm64-unknown-emscripten-draft_2024_02_15",
  admissableList: AdmissableList,
  WeaveDrive: weaveDrive,
  ARWEAVE: 'http://localhost:3001',
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
};

test('AoLoader should execute Lua code for multiple prompts and measure execution time', async () => {
  const env = {
    Process: {
      Id: 'AOS',
      Owner: 'FOOBAR',
      Tags: [
        { name: 'Name', value: 'Thomas' }
      ]
    }
  };
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
      local result = Llama.load("/data/${selectedModule}")
    `
  };

  // Initialize AoLoader with the wasm and options
  const handle = await AoLoader(wasm, options);
  const result = await handle(null, msg, env);

  const prompts = [
    "Once upon a time",
    "There was a man who",
    "I had an idea about"
  ];

  const executionTimes = [];

  // Measure execution time for each prompt
  for (let i = 0; i < prompts.length; i++) {
    const prompt = prompts[i];
    const startTime = performance.now();

    const luaCode = `
      local Llama = require(".Llama")
      io.stderr:write("Loaded! Setting prompt ${i + 1}...\\n")
      Llama.setPrompt("${prompt}")
      io.stderr:write("Prompt set! Running...\\n")
      local str = Llama.run(30)
      print(str)
      return "OK"
    `;
    msg.Data = luaCode;
    await handle(result.Memory, msg, env);

    const endTime = performance.now();
    const executionTime = (endTime - startTime) / 1000;
    console.log(`Execution Time for prompt ${i + 1}: ${executionTime} seconds`);

    executionTimes.push(executionTime);
  }

  // Measure total execution time
  const end = performance.now();
  const totalExecutionTime = (end - start) / 1000;
  console.log(`Total Execution Time: ${totalExecutionTime} seconds`);

  // Assert that execution times are collected
  assert.ok(executionTimes.length === prompts.length, 'Execution times for all prompts should be measured');
  assert.ok(totalExecutionTime < 10, 'Total execution time should be within acceptable range');
});

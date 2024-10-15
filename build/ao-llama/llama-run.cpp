#include "common.h"
#include "llama.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#define CTX_SIZE 2048
#define BATCH_SIZE 2048

// return a formatted string
#define log_format(fmt, ...) ({ \
    char *str; \
    asprintf(&str, fmt, ##__VA_ARGS__); \
    str; \
})

gpt_params params;
llama_model* model;
llama_batch batch;
llama_context * ctx;
int tks_processed = 0;

extern "C" bool l_llama_on_progress(float progress, void * user_data);
extern "C" void l_llama_on_log(enum ggml_log_level level, const char * text, void * user_data);

extern "C" int llama_load(char* model_path);
int llama_load(char* model_path) {
    params.model = model_path;
    params.n_ctx = CTX_SIZE;

    params.embedding = true;

    params.n_batch = BATCH_SIZE;
    // For non-causal models, batch size must be equal to ubatch size
    params.n_ubatch = params.n_batch;

    // init LLM
    llama_backend_init();

    // initialize the model
    llama_model_params model_params = llama_model_params_from_gpt_params(params);
    model_params.use_mmap = true;
    model = llama_load_model_from_file(params.model.c_str(), model_params);

    if (model == NULL) {
        fprintf(stderr , "%s: error: unable to load model\n" , __func__);
        return 1;
    }

    return 0;
}

int isCtxFull() {
    return tks_processed > llama_n_ctx(ctx);
}

void llama_reset_context() {
    tks_processed = 0;
    llama_batch_free(batch);
    llama_free(ctx);

    batch = llama_batch_init(BATCH_SIZE, 0, 1);

    // (Re-)initialize the context
    llama_context_params ctx_params = llama_context_params_from_gpt_params(params);

    ctx_params.seed  = 1234;
    ctx_params.n_ctx = CTX_SIZE;
    ctx_params.n_threads = params.n_threads;
    ctx_params.n_threads_batch = params.n_threads_batch == -1 ? params.n_threads : params.n_threads_batch;

    ctx = llama_new_context_with_model(model, ctx_params);

    if (ctx == NULL) {
        l_llama_on_log(GGML_LOG_LEVEL_ERROR, "error: failed to create the llama_context\n", NULL);
    }

    const int n_ctx_train = llama_n_ctx_train(model);
    const int n_ctx = llama_n_ctx(ctx);

    // if (llama_model_has_encoder(model) && llama_model_has_decoder(model)) {
    //     l_llama_on_log(GGML_LOG_LEVEL_ERROR, log_format("%s: computing embeddings in encoder-decoder models is not supported\n", __func__), NULL);
    //     return NULL;
    // }

    if (n_ctx > n_ctx_train) {
        l_llama_on_log(GGML_LOG_LEVEL_WARN, log_format("%s: warning: model was trained on only %d context tokens (%d specified)\n",
                __func__, n_ctx_train, n_ctx), NULL);
    }
}

extern "C" int llama_set_prompt(char* prompt);
int llama_set_prompt(char* prompt) {
    llama_reset_context();
    params.prompt = prompt;

    // tokenize the prompt
    std::vector<llama_token> tokens_list;
    tokens_list = ::llama_tokenize(ctx, params.prompt, true);

    // make sure the KV cache is big enough to hold all the prompt and generated tokens
    if (isCtxFull()) {
        l_llama_on_log(GGML_LOG_LEVEL_ERROR, "error: n_kv_req > n_ctx, the required KV cache size is not big enough\n", NULL);
        return 1;
    }

    // create a llama_batch
    // we use this object to submit token data for decoding
    batch = llama_batch_init(BATCH_SIZE, 0, 1);

    fprintf(stderr, "Starting to ingest prompt...\n");

    // evaluate the initial prompt
    for (size_t i = 0; i < tokens_list.size(); i++) {
        llama_batch_add(batch, tokens_list[i], i, { 0 }, false);
        tks_processed += 1;
    }

    // llama_decode will output logits only for the last token of the prompt
    batch.logits[batch.n_tokens - 1] = true;

    if (llama_decode(ctx, batch) != 0) {
        l_llama_on_log(GGML_LOG_LEVEL_ERROR, "Failed to eval, return code %d\n", NULL);
        return 1;
    }

    return 0;
}

extern "C" char* llama_next();
char* llama_next() {
    auto   n_vocab = llama_n_vocab(model);
    auto * logits  = llama_get_logits_ith(ctx, batch.n_tokens - 1);
    char* token = (char*)malloc(256);

    std::vector<llama_token_data> candidates;
    candidates.reserve(n_vocab);

    for (llama_token token_id = 0; token_id < n_vocab; token_id++) {
        candidates.emplace_back(llama_token_data{ token_id, logits[token_id], 0.0f });
    }

    llama_token_data_array candidates_p = { candidates.data(), candidates.size(), false };

    // sample the most likely token
    const llama_token new_token_id = llama_sample_token_greedy(ctx, &candidates_p);
    std::string token_str = llama_token_to_piece(ctx, new_token_id);
    strcpy(token, token_str.c_str());
    tks_processed += 1;

    // is it an end of generation?
    if (llama_token_is_eog(model, new_token_id)) {
        return 0;
    }

    // prepare the next batch
    llama_batch_clear(batch);

    // push this new token for next evaluation
    llama_batch_add(batch, new_token_id, tks_processed, { 0 }, true);

    // evaluate the current batch with the transformer model
    if (llama_decode(ctx, batch)) {
        l_llama_on_log(GGML_LOG_LEVEL_ERROR, "Failed to eval, return code %d\n", NULL);
        return 0;
    }

    return token;
}

extern "C" char* llama_run(int len);
char* llama_run(int len) {
    char* response = (char*)malloc(len * 256);

    for (int i = 0; i < len; i++) {
        // sample the next token
        char* next_token = llama_next();
        strcat(response, next_token);
        free(next_token);
    }

    return response;
}

extern "C" int llama_add(char* new_string);
int llama_add(char* new_string) {
    std::vector<llama_token> new_tokens_list = ::llama_tokenize(ctx, new_string, true);

    if (isCtxFull()) {
        l_llama_on_log(GGML_LOG_LEVEL_ERROR, "Context full, cannot add more tokens\n", NULL);
        return 1;
    }

    // Add new tokens to the batch
    for (size_t i = 0; i < new_tokens_list.size(); i++) {
        llama_batch_add(batch, new_tokens_list[i], tks_processed + i, {0}, false);
        tks_processed++;
    }

    batch.logits[batch.n_tokens - 1] = true;
    if (llama_decode(ctx, batch) != 0) {
        l_llama_on_log(GGML_LOG_LEVEL_ERROR, "llama_decode() failed with new tokens\n", NULL);
        return 1;
    }

    return 0;
}

static void batch_add_seq(llama_batch & batch, const std::vector<int32_t> & tokens, llama_seq_id seq_id) {
    size_t n_tokens = tokens.size();
    for (size_t i = 0; i < n_tokens; i++) {
        llama_batch_add(batch, tokens[i], i, { seq_id }, true);
    }
}

static void batch_decode(llama_context * ctx, llama_batch & batch, float * output, int n_seq, int n_embd, int embd_norm) {
    // clear previous kv_cache values (irrelevant for embeddings)
    llama_kv_cache_clear(ctx);

    // run model
    l_llama_on_log(GGML_LOG_LEVEL_WARN, log_format("%s: n_tokens = %d, n_seq = %d\n", __func__, batch.n_tokens, n_seq), NULL);
    if (llama_decode(ctx, batch) < 0) {
        l_llama_on_log(GGML_LOG_LEVEL_WARN, log_format("%s : failed to decode\n", __func__), NULL);
    }

    for (int i = 0; i < batch.n_tokens; i++) {
        if (!batch.logits[i]) {
            continue;
        }

        // try to get sequence embeddings - supported only when pooling_type is not NONE
        const float * embd = llama_get_embeddings_seq(ctx, batch.seq_id[i][0]);
        GGML_ASSERT(embd != NULL && "failed to get sequence embeddings");

        float * out = output + batch.seq_id[i][0] * n_embd;
        llama_embd_normalize(embd, out, n_embd, embd_norm);
    }
}

extern "C" char* llama_embed(char* text);
char* llama_embed(char* text) {
    llama_reset_context();

    const enum llama_pooling_type pooling_type = llama_pooling_type(ctx);
    if (pooling_type == LLAMA_POOLING_TYPE_NONE) {
        l_llama_on_log(GGML_LOG_LEVEL_ERROR, log_format("%s: error: pooling type NONE not supported\n", __func__), NULL);
        return NULL;
    }

    // use text as the only prompt
    std::vector<std::string> prompts = { text };

    // max batch size
    const uint64_t n_batch = params.n_batch;
    GGML_ASSERT(params.n_batch >= params.n_ctx);

    // tokenize the prompts and trim
    std::vector<std::vector<int32_t>> inputs;
    for (const auto & prompt : prompts) {
        auto inp = ::llama_tokenize(ctx, prompt, true, false);
        if (inp.size() > n_batch) {
            l_llama_on_log(GGML_LOG_LEVEL_ERROR, log_format("%s: error: number of tokens in input line (%lld) exceeds batch size (%lld), increase batch size and re-run\n",
                    __func__, (long long int) inp.size(), (long long int) n_batch), NULL);
            return NULL;
        }
        inputs.push_back(inp);
    }

    // check if the last token is SEP
    // it should be automatically added by the tokenizer when 'tokenizer.ggml.add_eos_token' is set to 'true'
    for (auto & inp : inputs) {
        if (inp.empty() || inp.back() != llama_token_sep(model)) {
            l_llama_on_log(GGML_LOG_LEVEL_WARN, log_format("%s: warning: last token in the prompt is not SEP\n", __func__), NULL);
            l_llama_on_log(GGML_LOG_LEVEL_WARN, log_format("%s:          'tokenizer.ggml.add_eos_token' should be set to 'true' in the GGUF header\n", __func__), NULL);
        }
    }

    // // tokenization stats
    // if (params.verbose_prompt) {
    //     for (int i = 0; i < (int) inputs.size(); i++) {
    //         fprintf(stderr, "%s: prompt %d: '%s'\n", __func__, i, prompts[i].c_str());
    //         fprintf(stderr, "%s: number of tokens in prompt = %zu\n", __func__, inputs[i].size());
    //         for (int j = 0; j < (int) inputs[i].size(); j++) {
    //             fprintf(stderr, "%6d -> '%s'\n", inputs[i][j], llama_token_to_piece(ctx, inputs[i][j]).c_str());
    //         }
    //         fprintf(stderr, "\n\n");
    //     }
    // }

    // initialize batch
    const int n_prompts = prompts.size();
    struct llama_batch batch = llama_batch_init(n_batch, 0, 1);

    // allocate output
    const int n_embd = llama_n_embd(model);
    std::vector<float> embeddings(n_prompts * n_embd, 0);
    float * emb = embeddings.data();

    // break into batches
    int p = 0; // number of prompts processed already
    int s = 0; // number of prompts in current batch
    for (int k = 0; k < n_prompts; k++) {
        // clamp to n_batch tokens
        auto & inp = inputs[k];

        const uint64_t n_toks = inp.size();

        // encode if at capacity
        if (batch.n_tokens + n_toks > n_batch) {
            float * out = emb + p * n_embd;
            batch_decode(ctx, batch, out, s, n_embd, params.embd_normalize);
            llama_batch_clear(batch);
            p += s;
            s = 0;
        }

        // add to batch
        batch_add_seq(batch, inp, s);
        s += 1;
    }

    // final batch
    float * out = emb + p * n_embd;
    batch_decode(ctx, batch, out, s, n_embd, params.embd_normalize);

    // l_llama_on_log(GGML_LOG_LEVEL_INFO, "Embedding done\n", NULL);
    // l_llama_on_log(GGML_LOG_LEVEL_INFO,
    //     log_format(
    //         "n_prompts = %d, n_embd = %d, embd_normalize = %d\n",
    //         n_prompts, n_embd, params.embd_normalize
    //     ),
    // NULL);

    char* output = (char*)malloc(2048 * 8);
    int output_len = 0;
    // print the first part of the embeddings or for a single prompt, the full embedding
    // fprintf(stdout, "\n");
    for (int j = 0; j < n_prompts; j++) {
        // fprintf(stdout, "embedding %d: ", j);
        for (int i = 0; i < n_embd; i++) {
            // fprintf(stdout, "%9.6f ", emb[j * n_embd + i]);
            output_len += sprintf(output + output_len, "%.8f", emb[j * n_embd + i]);
            
            // Add comma if not last element
            if (i < n_embd - 1) {
                output_len += sprintf(output + output_len, ",");
            }
        }
        // fprintf(stdout, "DONE\n");
    }

    // // print cosine similarity matrix
    // if (n_prompts > 1) {
    //     fprintf(stdout, "\n");
    //     printf("cosine similarity matrix:\n\n");
    //     for (int i = 0; i < n_prompts; i++) {
    //         fprintf(stdout, "%6.6s ", prompts[i].c_str());
    //     }
    //     fprintf(stdout, "\n");
    //     for (int i = 0; i < n_prompts; i++) {
    //         for (int j = 0; j < n_prompts; j++) {
    //             float sim = llama_embd_similarity_cos(emb + i * n_embd, emb + j * n_embd, n_embd);
    //             fprintf(stdout, "%6.2f ", sim);
    //         }
    //         fprintf(stdout, "%1.10s", prompts[i].c_str());
    //         fprintf(stdout, "\n");
    //     }
    // }
    
    return output;

    // // output string
    // char* output = (char*)malloc(2048 * 8);
    // output[0] = '\0';
    // for (int i = 0; i < n_embd; i++) {
    //     if (params.embd_normalize == 0) {
    //         strcat(output, log_format("%6.0f ", emb[n_embd + i]));
    //     } else {
    //         strcat(output, log_format("%9.6f ", emb[n_embd + i]));
    //     }
    // }
    // return output;
}

extern "C" void llama_stop();
void llama_stop() {
    llama_free(ctx);
    llama_batch_free(batch);
    llama_free_model(model);
    llama_backend_free();
}
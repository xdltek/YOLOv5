/**
 * @file parser_api.h
 * @brief Helper for loading an ONNX file into the RppRT ONNX parser.
 */


#ifndef RPPRT_PARSER_API_H
#define RPPRT_PARSER_API_H

#include <fstream>
#include <iostream>
#include <vector>

#include "OnnxParser.h"
#include "logging.h"

/**
 * @brief Read an ONNX model file and parse it into an existing runtime network definition.
 * @param onnx_filename Path to the ONNX model file.
 * @param builder Runtime builder associated with the target network.
 * @param network Network definition populated by the parser.
 * @param parser ONNX parser instance used to report parse diagnostics.
 */
inline int onnx_parser(std::string onnx_filename, infer1::IBuilder *builder,
                infer1::INetworkDefinition *network, onnxparser::IParser *parser)
{
    // The builder is kept in the signature to match existing caller code and parser ownership flow.
    (void)builder;
    (void)network;

    // Read the full model into memory because the parser consumes a contiguous buffer.
    std::ifstream onnx_file(onnx_filename.c_str(),
                            std::ios::binary | std::ios::ate);
    std::streamsize file_size = onnx_file.tellg();
    onnx_file.seekg(0, std::ios::beg);
    std::vector<char> onnx_buf(file_size);
    if (!onnx_file.read(onnx_buf.data(), onnx_buf.size())) {
        sample::LOG_ERROR() << "ERROR: Failed to read from file: " << onnx_filename << std::endl;
        return -4;
    }
    // Forward parser diagnostics through the shared logger so model errors are visible to users.
    if (!parser->parse(onnx_buf.data(), onnx_buf.size())) {
        int nerror = parser->getNbErrors();
        for (int i = 0; i < nerror; ++i) {
            onnxparser::IParserError const* error = parser->getError(i);
            sample::LOG_ERROR() << "ERROR: "
                << error->file() << ":" << error->line()
                << " In function " << error->func() << ":\n"
                << "[" << static_cast<int>(error->code()) << "] " << error->desc()
                << std::endl;
        }

        // Preserve the original return-code behavior used by the surrounding sample code.
        if (nerror != static_cast<int>(onnxparser::ErrorCode::kSUCCESS))
        {
            return -1;
        }
        return -5;
    }
    return 0;
}

#endif // RPPRT_PARSER_API_H

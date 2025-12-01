#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <json/json.h>
#include "../src/FileOpController.hpp"

#define ASSERT_TRUE(cond) if(!(cond)) { std::cerr << "Assertion failed: " << #cond << " at " << __FILE__ << ":" << __LINE__ << std::endl; return 1; }

int main() {
    try {
        FileOpController controller;
        auto tmpDir = std::filesystem::temp_directory_path();
        auto tmpFile = tmpDir / "mcp_fileop_test.txt";
        std::string content = "Hello, FileOpController!";

        // Write a temp file
        std::ofstream ofs(tmpFile);
        ofs << content;
        ofs.close();

        // Preload via fileop tool
        Json::Value preloadParams;
        preloadParams["name"] = "fileop";
        preloadParams["arguments"]["operation"] = "preload";
        preloadParams["arguments"]["path"] = tmpFile.string();
        Json::Value preloadRes = controller.callTool(preloadParams);
        ASSERT_TRUE(!preloadRes.isMember("__error__"));
        ASSERT_TRUE(preloadRes.isMember("content"));

        // Canonical handler
        auto handler = std::filesystem::canonical(tmpFile).string();

        // Verify resource appears in listResources
        Json::Value resources = controller.listResources();
        ASSERT_TRUE(resources.isMember("resources"));
        bool found = false;
        for (auto& r : resources["resources"]) {
            if (r["uri"].asString() == std::string("file:///") + handler) {
                found = true;
                break;
            }
        }
        ASSERT_TRUE(found);

        // Read via callTool read operation
        Json::Value readParams;
        readParams["name"] = "fileop";
        readParams["arguments"]["operation"] = "read";
        readParams["arguments"]["handler"] = handler;
        readParams["arguments"]["offset"] = (Json::UInt64)0;
        readParams["arguments"]["size"] = (Json::UInt64)content.size();
        readParams["arguments"]["format"] = "text";
        Json::Value readRes = controller.callTool(readParams);
        ASSERT_TRUE(!readRes.isMember("__error__"));
        // read now returns a content[] array with parts[] inside; check the first part
        ASSERT_TRUE(readRes.isMember("content"));
        ASSERT_TRUE(readRes["content"][0]["parts"][0]["text"].asString() == content);

        // No longer support stream_read; use read or read_multiple

        // Close the handler
        Json::Value closeParams;
        closeParams["name"] = "fileop";
        closeParams["arguments"]["operation"] = "close";
        closeParams["arguments"]["handler"] = handler;
        Json::Value closeRes = controller.callTool(closeParams);
        ASSERT_TRUE(!closeRes.isMember("__error__"));

        // Resource should be removed
        Json::Value resourcesAfter = controller.listResources();
        bool stillFound = false;
        for (auto& r : resourcesAfter["resources"]) {
            if (r["uri"].asString() == std::string("file:///") + handler) {
                stillFound = true;
                break;
            }
        }
        ASSERT_TRUE(!stillFound);

        // Test legacy preload format: top-level name "preload"
        // reuse tmpFile; ensure it's present
        std::ofstream ofs2(tmpFile);
        ofs2 << content;
        ofs2.close();
        Json::Value legacyPreload;
        legacyPreload["name"] = "preload";
        legacyPreload["arguments"]["path"] = tmpFile.string();
        Json::Value legacyRes = controller.callTool(legacyPreload);
        ASSERT_TRUE(!legacyRes.isMember("__error__"));
        
            // Test read_multiple across multiple ranges and handlers
            // reuse tmpFile created earlier; just ensure it exists
            std::ofstream ofs3(tmpFile);
            ofs3 << content;
            ofs3.close();
            // Preload again
            Json::Value legacyPreload2;
            legacyPreload2["name"] = "preload";
            legacyPreload2["arguments"]["path"] = tmpFile.string();
            Json::Value legacyRes2 = controller.callTool(legacyPreload2);
            ASSERT_TRUE(!legacyRes2.isMember("__error__"));
            std::string handler2 = std::filesystem::canonical(tmpFile).string();
            // prepare a read_multiple that requests two ranges: prefix and suffix
            Json::Value rm;
            rm["name"] = "fileop";
            rm["arguments"]["operation"] = "read_multiple";
            Json::Value seg;
            seg["handler"] = handler2;
            seg["ranges"][0]["offset"] = (Json::UInt64)0;
            seg["ranges"][0]["size"] = (Json::UInt64)5;
            seg["ranges"][1]["offset"] = (Json::UInt64)(content.size() - 6);
            seg["ranges"][1]["size"] = (Json::UInt64)6;
            rm["arguments"]["segments"].append(seg);
            std::vector<double> rmProgress;
            auto rmCb = [&rmProgress](const Json::Value& p) { rmProgress.push_back(p["progress"].asDouble()); };
            Json::Value rmRes = controller.callTool(rm, rmCb);
            ASSERT_TRUE(!rmRes.isMember("__error__"));
            ASSERT_TRUE(rmRes.isMember("content"));
            ASSERT_TRUE(rmRes["content"][0]["parts"].size() == 2);
            ASSERT_TRUE(!rmProgress.empty());

                // Also validate legacy top-level name 'read_multiple'
                Json::Value rmLegacy;
                rmLegacy["name"] = "read_multiple";
                rmLegacy["arguments"]["segments"] = rm["arguments"]["segments"];
                std::vector<double> rmLegacyProg;
                auto rmLegacyCb = [&rmLegacyProg](const Json::Value& p) { rmLegacyProg.push_back(p["progress"].asDouble()); };
                Json::Value rmLegacyRes = controller.callTool(rmLegacy, rmLegacyCb);
                ASSERT_TRUE(!rmLegacyRes.isMember("__error__"));
                ASSERT_TRUE(!rmLegacyProg.empty());
            // Test lines format with multiple newline types
            std::string lineContent = "L1\nL2\r\nL3\nL4";
            auto tmpFile2 = tmpDir / "mcp_fileop_lines_test.txt";
            std::ofstream ofs4(tmpFile2);
            ofs4 << lineContent;
            ofs4.close();
            // preload
            Json::Value preloadLines;
            preloadLines["name"] = "preload";
            preloadLines["arguments"]["path"] = tmpFile2.string();
            Json::Value preloadLinesRes = controller.callTool(preloadLines);
            ASSERT_TRUE(!preloadLinesRes.isMember("__error__"));
            std::string handlerLines = std::filesystem::canonical(tmpFile2).string();
            // read multiple lines starting from line 1, read 2 lines -> L2 + L3
            Json::Value rmLines;
            rmLines["name"] = "fileop";
            rmLines["arguments"]["operation"] = "read_multiple";
            Json::Value segLines;
            segLines["handler"] = handlerLines;
            segLines["format"] = "lines";
            segLines["ranges"][0]["offset"] = (Json::UInt64)1;
            segLines["ranges"][0]["size"] = (Json::UInt64)2;
            rmLines["arguments"]["segments"].append(segLines);
            std::vector<double> rmLinesProgress;
            auto rmLinesCb = [&rmLinesProgress](const Json::Value& p) { rmLinesProgress.push_back(p["progress"].asDouble()); };
            Json::Value rmLinesRes = controller.callTool(rmLines, rmLinesCb);
            ASSERT_TRUE(!rmLinesRes.isMember("__error__"));
            ASSERT_TRUE(rmLinesRes.isMember("content"));
            ASSERT_TRUE(rmLinesRes["content"][0]["parts"].size() == 1);
            std::string expected = "L2\r\nL3\n";
            ASSERT_TRUE(rmLinesRes["content"][0]["parts"][0]["text"].asString() == expected);
            ASSERT_TRUE(!rmLinesProgress.empty());
        // Cleanup file
        std::filesystem::remove(tmpFile);

    } catch (const std::exception& e) {
        std::cerr << "Exception in test: " << e.what() << std::endl;
        return 1;
    }
    std::cout << "All FileOpController tests passed" << std::endl;
    return 0;
}

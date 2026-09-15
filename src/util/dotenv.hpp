#pragma once
#include <string>

// Loads KEY=VALUE pairs from a .env file into the process environment.
// Existing environment variables are never overwritten.
void load_dotenv(const std::string& path = ".env");

#include <sstream>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <chrono>
#include <fstream>
#include <random>

class MultiThreadedData {
private:
    int fields[3]; // Three integer fields
    std::mutex mutexes[3]; // Mutexes for each field

public:
    MultiThreadedData() {
        for (int i = 0; i < 3; ++i) {
            fields[i] = 0; // Initialize fields to 0
        }
    }

    void write(int index, int value) {
        if (index < 0 || index >= 3) return;
        std::lock_guard<std::mutex> lock(mutexes[index]);
        fields[index] = value;
    }

    int read(int index) {
        if (index < 0 || index >= 3) return -1;
        std::lock_guard<std::mutex> lock(mutexes[index]);
        return fields[index];
    }

    std::string to_string() {
        std::string result = "Fields: [";
        for (int i = 0; i < 3; ++i) {
            result += std::to_string(fields[i]);
            if (i < 2) result += ", ";
        }
        result += "]";
        return result;
    }
};

void generateActionSequence(const std::string& filename, int totalActions) {
    std::ofstream file(filename);
    std::default_random_engine generator;
    std::uniform_int_distribution<int> fieldDist(0, 2);
    std::uniform_int_distribution<int> valueDist(1, 100);

    for (int i = 0; i < totalActions; ++i) {
        int actionType = std::uniform_int_distribution<int>(1, 100)(generator);
        if (actionType <= 10) { // 10% for write 0
            file << "write 0 " << valueDist(generator) << '\n';
        }
        else if (actionType <= 20) { // 10% for write 1
            file << "write 1 " << valueDist(generator) << '\n';
        }
        else if (actionType <= 30) { // 40% for read 2
            file << "read 2\n";
        }
        else if (actionType <= 35) { // 5% for write 2
            file << "write 2 " << valueDist(generator) << '\n';
        }
        else if (actionType <= 50) { // 10% for read 0
            file << "read 0\n";
        }
        else if (actionType <= 60) { // 10% for read 1
            file << "read 1\n";
        }
        else { // 15% for string
            file << "string\n";
        }
    }

    file.close();
}

// Function to measure execution time and log results
void measureExecutionTime(MultiThreadedData& data, const std::string& filename) {
    auto start = std::chrono::high_resolution_clock::now();
    executeActions(data, filename);
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;

    // Print to console
    std::cout << "Execution time for " << filename << ": " << duration.count() << " seconds\n";

    // Append execution time to the action file
    std::ofstream actionFile(filename, std::ios_base::app); // Open in append mode
    if (actionFile.is_open()) {
        actionFile << "Execution time: " << duration.count() << " seconds\n";
        actionFile.close();
    }
    else {
        std::cerr << "Unable to open action file: " << filename << std::endl;
    }
}

int main() {
    MultiThreadedData data;
    generateActionSequence("actions1.txt", 1000); // Adjust totalActions as needed
    generateActionSequence("actions2.txt", 1000);
    generateActionSequence("actions3.txt", 1000);

    // Measure execution time for single-threaded
    measureExecutionTime(data, "actions1.txt");

    // Measure execution time for two threads
    std::thread t1(measureExecutionTime, std::ref(data), "actions2.txt");
    std::thread t2(measureExecutionTime, std::ref(data), "actions3.txt");
    t1.join();
    t2.join();

    // Measure execution time for three threads
    std::thread t3(measureExecutionTime, std::ref(data), "actions1.txt");
    std::thread t4(measureExecutionTime, std::ref(data), "actions2.txt");
    std::thread t5(measureExecutionTime, std::ref(data), "actions3.txt");
    t3.join();
    t4.join();
    t5.join();

    MultiThreadedData data;
    std::ifstream actionFile;
    std::string line;

    // Read and execute actions from each generated file
    for (int i = 1; i <= 3; ++i) {
        actionFile.open("actions" + std::to_string(i) + ".txt");
        while (std::getline(actionFile, line)) {
            std::istringstream iss(line);
            std::string command;
            int index, value;
            if (iss >> command) {
                if (command == "write") {
                    iss >> index >> value;
                    data.write(index, value);
                }
                else if (command == "read") {
                    iss >> index;
                    std::cout << "Read from field " << index << ": " << data.read(index) << '\n';
                }
                else if (command == "string") {
                    std::cout << data.to_string() << '\n';
                }
            }
        }
        actionFile.close();
    }

    return 0;
}
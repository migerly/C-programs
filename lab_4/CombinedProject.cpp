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
    int fields[3];            // три цілі поля
    std::mutex mutexes[3];    // по одному м'ютексу на кожне поле

public:
    MultiThreadedData() {
        for (int i = 0; i < 3; ++i) {
            fields[i] = 0;
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
        // для коректного знімка стану — можна заблокувати всі три м’ютекси
        std::scoped_lock lock(mutexes[0], mutexes[1], mutexes[2]);

        std::string result = "Fields: [";
        for (int i = 0; i < 3; ++i) {
            result += std::to_string(fields[i]);
            if (i < 2) result += ", ";
        }
        result += "]";
        return result;
    }
};

// генерація послідовності дій згідно частот варіанта №9
// (m = 3, read/write для кожного поля + string)
void generateActionSequence(const std::string& filename, int totalActions) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Cannot open file " << filename << " for writing\n";
        return;
    }

    std::default_random_engine generator(std::random_device{}());
    std::uniform_int_distribution<int> rollDist(1, 100);
    std::uniform_int_distribution<int> valueDist(1, 100);

    for (int i = 0; i < totalActions; ++i) {
        int r = rollDist(generator);

        if (r <= 10) {                   // 10% read 0
            file << "read 0\n";
        }
        else if (r <= 20) {            // 10% write 0
            file << "write 0 " << valueDist(generator) << '\n';
        }
        else if (r <= 30) {            // 10% read 1
            file << "read 1\n";
        }
        else if (r <= 40) {            // 10% write 1
            file << "write 1 " << valueDist(generator) << '\n';
        }
        else if (r <= 80) {            // 40% read 2
            file << "read 2\n";
        }
        else if (r <= 85) {            // 5% write 2
            file << "write 2 " << valueDist(generator) << '\n';
        }
        else {                         // 15% string
            file << "string\n";
        }
    }
}

// читаємо файл дій і виконуємо їх над спільною структурою
void executeActions(MultiThreadedData& data, const std::string& filename) {
    std::ifstream actionFile(filename);
    if (!actionFile.is_open()) {
        std::cerr << "Unable to open action file: " << filename << std::endl;
        return;
    }

    std::string line;
    while (std::getline(actionFile, line)) {
        std::istringstream iss(line);
        std::string command;
        int index, value;

        if (!(iss >> command))
            continue;

        if (command == "write") {
            if (iss >> index >> value) {
                data.write(index, value);
            }
        }
        else if (command == "read") {
            if (iss >> index) {
                std::cout << "Read from field " << index
                    << ": " << data.read(index) << '\n';
            }
        }
        else if (command == "string") {
            std::cout << data.to_string() << '\n';
        }
    }

    actionFile.close();
}

// вимірювання часу виконання послідовності дій з файлу
// (час зчитування файлу можна винести окремо, але тут хоч базово є замір алгоритму)
void measureExecutionTime(MultiThreadedData& data, const std::string& filename) {
    // читаємо файл наперед, щоб (хоч приблизно) не враховувати IO у замір
    std::ifstream actionFile(filename);
    if (!actionFile.is_open()) {
        std::cerr << "Unable to open action file for timing: " << filename << std::endl;
        return;
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(actionFile, line)) {
        lines.push_back(line);
    }
    actionFile.close();

    auto start = std::chrono::high_resolution_clock::now();

    for (const auto& l : lines) {
        std::istringstream iss(l);
        std::string command;
        int index, value;

        if (!(iss >> command))
            continue;

        if (command == "write") {
            if (iss >> index >> value) {
                data.write(index, value);
            }
        }
        else if (command == "read") {
            if (iss >> index) {
                data.read(index); // для замірів достатньо читання, можна не друкувати
            }
        }
        else if (command == "string") {
            data.to_string(); // теж без друку, щоб не спотворювати час
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;

    std::cout << "Execution time for " << filename
        << ": " << duration.count() << " seconds\n";
}

int main() {
    // спільна структура даних, по якій працюють усі потоки
    MultiThreadedData data;

    // генеруємо три файли з діями (тут поки всі – з частотами варіанта №9)
    generateActionSequence("actions1.txt", 100000);
    generateActionSequence("actions2.txt", 100000);
    generateActionSequence("actions3.txt", 100000);

    std::cout << "=== 1 thread ===\n";
    measureExecutionTime(data, "actions1.txt");

    std::cout << "\n=== 2 threads ===\n";
    std::thread t1(measureExecutionTime, std::ref(data), "actions2.txt");
    std::thread t2(measureExecutionTime, std::ref(data), "actions3.txt");
    t1.join();
    t2.join();

    std::cout << "\n=== 3 threads ===\n";
    std::thread t3(measureExecutionTime, std::ref(data), "actions1.txt");
    std::thread t4(measureExecutionTime, std::ref(data), "actions2.txt");
    std::thread t5(measureExecutionTime, std::ref(data), "actions3.txt");
    t3.join();
    t4.join();
    t5.join();

    return 0;
}

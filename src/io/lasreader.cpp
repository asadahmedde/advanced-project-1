// lasreader.cpp

#include <iostream>
#include <thread>
#include <cmath>
#include <vector>

#include "lasreader.h"
#include "liblas/liblas.hpp"

// Constructor
LASReader::LASReader(int numThreads, pcl::PointCloud<PointDefaultType>::Ptr cloud) : m_NumberOfThreads(numThreads), m_PointCloud(cloud)
{

}

// Read in PC using threads
bool LASReader::ReadLASPointCloud(const std::string &lasFilePath)
{
    // input file check
    std::ifstream is { lasFilePath.c_str(), std::ios::in | std::ios::binary };
    if (is.fail()) {
        std::cerr << "\nERROR: Failed to open file: " << lasFilePath << std::endl;
        return false;
    }

    // read header into
    liblas::ReaderFactory rf;
    liblas::Reader reader { rf.CreateWithStream(is) };
    unsigned long int pointCount = reader.GetHeader().GetPointRecordsCount();

    is.close();
    std::cout << "\nNumber of points in LAS file: " << pointCount << std::endl;

    // create required number of threads and assign each its range to process
    std::vector<std::thread> threads;
    unsigned long int pointsPerThread = pointCount / static_cast<unsigned long int>(m_NumberOfThreads);

    // setup threads
    for (int i = 0; i < m_NumberOfThreads; i++)
    {
        unsigned long int start = (static_cast<unsigned long int>(i) * pointsPerThread);
        unsigned long int end = start + pointsPerThread;
        //std::thread t([=] { this->ProcessFileTask(start, end, lasFilePath.c_str());});
        threads.emplace_back(std::thread([=] { this->ProcessFileTask(start, end, lasFilePath.c_str());}));
        //threads[i].detach();
        //std::thread t(&LASReader::ProcessFileTask, this, start, end, lasFilePath.c_str(), pc);
    }

    for (size_t i = 0; i < threads.size(); i++) {
        threads[i].join();
    }

    std::cout << "\nAll LAS reader threads finished reading the file.\n";
    std::cout.flush();

    return true;
}

// Thread reading task
void LASReader::ProcessFileTask(unsigned long start, unsigned long end, const char* filePath)
{
    //std::cout << "\nThread " << std::this_thread::get_id() << " starting file read task" << std::endl;

    std::ifstream is { filePath };
    if (is.fail()) {
        std::cerr << "\nERROR: LAS reader thread failed to launch." << std::endl;
        return;
    }

    liblas::ReaderFactory rf;
    liblas::Reader reader = rf.CreateWithStream(is);

    reader.Seek(start);
    unsigned long int readCount = 0;
    while (reader.ReadNextPoint() && (readCount < (start + end)))
    {
        pcl::PointXYZRGB point;
        ConvertToPCDPoint(reader.GetPoint(), point);

        m_Mutex.lock();
        m_PointCloud->push_back(point);
        m_Mutex.unlock();

        readCount++;
    }

    is.close();

    std::cout << "\nThread " << std::this_thread::get_id() << " finished reading from " << start << " to " << end << std::endl;
}

// Convert 16bit r,g,b values into PCL point data type
void LASReader::ConvertToPCDPoint(const liblas::Point& lasPoint, PointDefaultType& pclPoint)
{
    float r1 = static_cast<float>(lasPoint.GetColor().GetRed());
    float g1 = static_cast<float>(lasPoint.GetColor().GetGreen());
    float b1 = static_cast<float>(lasPoint.GetColor().GetBlue());

    const float BIT_16_MAX = 65536.0f;
    const float BIT_8_MAX = 255.0f;

    uint8_t r2 = static_cast<uint8_t>((r1 / BIT_16_MAX) * BIT_8_MAX);
    uint8_t g2 = static_cast<uint8_t>((g1 / BIT_16_MAX) * BIT_8_MAX);
    uint8_t b2 = static_cast<uint8_t>((b1 / BIT_16_MAX) * BIT_8_MAX);

    pclPoint.x = static_cast<float>(lasPoint.GetX());
    pclPoint.y = static_cast<float>(lasPoint.GetY());
    pclPoint.z = static_cast<float>(lasPoint.GetZ());

    pclPoint.r = r2; pclPoint.g = g2; pclPoint.b = b2;
}
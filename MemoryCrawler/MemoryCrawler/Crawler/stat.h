//
//  stat.h
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/19.
//  Copyright © 2019 larryhou. All rights reserved.
//

#ifndef stat_h
#define stat_h

#include "types.h"
#include <vector>
#include <map>
#include <stdio.h>
#include <math.h>

template <class T>
class Statistics
{
    std::vector<T> __samples;
    
public:
    constexpr static float CI95_TAIL = 1.28156;
    constexpr static float CI95_BODY = 1.64489;
    
    T minimum;
    T maximum;
    
    T reasonableMinimum;
    T reasonableMaximum;
    
    T sum;
    
    double mean;
    double standardDeviation;
    
public:
    Statistics();
    void collect(T sample);
    void summarize();
    void clear();
    int32_t size();
    
    void iterateUnusualMaximums(std::function<void(int32_t, T)> callback);
    void iterateUnusualMinimums(std::function<void(int32_t, T)> callback);
};

template <class T>
Statistics<T>::Statistics()
{
    static_assert(std::is_arithmetic<T>::value, "NOT summarizable type");
}

template <class T>
void Statistics<T>::collect(T sample)
{
    __samples.push_back(sample);
}

template <class T>
void Statistics<T>::summarize()
{
    sum = 0;
    
    minimum = __samples[0];
    maximum = 0;
    for (auto iter = __samples.begin(); iter != __samples.end(); iter++)
    {
        auto v = *iter;
        sum += v;
        if (v > maximum) {maximum = v;}
        if (v < minimum) {minimum = v;}
    }
    
    mean = (double)sum / (double)__samples.size();
    
    double variance = 0;
    for (auto iter = __samples.begin(); iter != __samples.end(); iter++)
    {
        variance += pow((double)*iter - mean, 2);
    }
    
    standardDeviation = pow(variance / (double)(__samples.size() - 1), 0.5);
    auto upper = mean + 3 * standardDeviation;
    auto lower = mean - 3 * standardDeviation;
    
    reasonableMaximum = minimum;
    reasonableMinimum = maximum;
    for (auto iter = __samples.begin(); iter != __samples.end(); iter++)
    {
        auto v = *iter;
        if (v > reasonableMaximum && v <= upper) {reasonableMaximum = v;}
        if (v < reasonableMinimum && v >= lower) {reasonableMinimum = v;}
    }
}

template <class T>
void Statistics<T>::iterateUnusualMaximums(std::function<void (int32_t/*index*/, T)> callback)
{
    int32_t index = 0;
    for (auto iter = __samples.begin(); iter != __samples.end(); iter++)
    {
        auto value = *iter;
        if (value > reasonableMaximum) { callback(index, value); }
        ++index;
    }
}

template <class T>
void Statistics<T>::iterateUnusualMinimums(std::function<void (int32_t/*index*/, T)> callback)
{
    int32_t index = 0;
    for (auto iter = __samples.begin(); iter != __samples.end(); iter++)
    {
        auto value = *iter;
        if (value < reasonableMinimum) { callback(index, value); }
        ++index;
    }
}

template <class T>
int32_t Statistics<T>::size()
{
    return (int32_t)__samples.size();
}

template <class T>
void Statistics<T>::clear()
{
    __samples.clear();
}

class TrackStatistics
{
    using sample_t = std::tuple<int32_t/*element_index*/, int32_t/*type_index*/, int32_t/*element_size*/>;
    
private:
    std::vector<sample_t> __samples;
    std::map<int32_t, int32_t> __memory;
    
public:
    void summarize(bool reverse = false);
    void collect(int32_t itemIndex, int32_t typeIndex, int32_t size);
    void foreach(std::function<void(int32_t/*itemIndex*/, int32_t/*typeIndex*/, int32_t/*typeMemory*/, uint64_t/*detail*/)> callback, int32_t depth = 10);
    void reverse();
};

template <class T>
struct IterRange
{
    T begin;
    T end;
};

#endif /* stat_h */

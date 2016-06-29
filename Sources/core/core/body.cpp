#include "charm.h"

#include "random.h"

#include "spike.h"
#include "body.h"

Body *Body::CreateBody(const std::string &type, json &params)
{
    if (type == RandomBody::Type) {
        return new RandomBody(params);
    } else {
        return nullptr;
    }
}

Body::Body(json &params)
{
    if (!params.empty()) {
        json sensors, actuators;
        for (auto itParams = params.begin(); itParams != params.end(); ++itParams) {
            if (itParams.key() == "sensors" && itParams->is_array()) {
                sensors = itParams.value();
            } else if (itParams.key() == "actuators" && itParams->is_array()) {
                actuators = itParams.value();
            }
        }

        for (auto itSensor = sensors.begin(); itSensor != sensors.end(); ++itSensor) {
            if (itSensor->is_object()) {

                std::string sensorName;
                size_t elementSize = 0;
                size_t elementCount = 0;
                for (auto it = itSensor->begin(); it != itSensor->end(); ++it) {
                    if (it.key() == "name" && it.value().is_string()) {
                        sensorName = it.value().get<std::string>();
                    } else if (it.key() == "spikeType" && it.value().is_string()) {
                        Spike::Data dummySpike;
                        Spike::Initialize(Spike::ParseType(it.value().get<std::string>()), 0, dummySpike);
                        elementSize = Spike::Edit(dummySpike)->AllBytes(dummySpike);
                    } else if (it.key() == "size" && it.value().is_number_integer()) {
                        elementCount = it.value().get<size_t>();
                    }
                }

                if (!sensorName.empty()) {
                    mSensorsInfo.insert(std::make_pair(sensorName,
                        ExpectedDataInfo(elementSize, elementCount)));
                }
            }
        }

        for (auto itActuator = actuators.begin(); itActuator != actuators.end(); ++itActuator) {
            if (itActuator->is_object()) {

                std::string actuatorName;
                size_t elementSize = 0;
                size_t elementCount = 0;
                for (auto it = itActuator->begin(); it != itActuator->end(); ++it) {
                    if (it.key() == "name" && it.value().is_string()) {
                        actuatorName = it.value().get<std::string>();
                    } else if (it.key() == "spikeType" && it.value().is_string()) {
                        Spike::Data dummySpike;
                        Spike::Initialize(Spike::ParseType(it.value().get<std::string>()), 0, dummySpike);
                        elementSize = Spike::Edit(dummySpike)->AllBytes(dummySpike);
                    } else if (it.key() == "size" && it.value().is_number_integer()) {
                        elementCount = it.value().get<size_t>();
                    }
                }

                if (!actuatorName.empty()) {
                    mActuatorsInfo.insert(std::make_pair(actuatorName,
                        ExpectedDataInfo(elementSize, elementCount)));
                }
            }
        }
    }
}

void Body::pup(PUP::er &p)
{
    p | mSensorsInfo;
    p | mActuatorsInfo;
}

const char *RandomBody::Type = "RandomBody";

RandomBody::RandomBody(json &params) : Body(params)
{
}

RandomBody::~RandomBody()
{
}

void RandomBody::pup(PUP::er &p)
{
    Body::pup(p);
}

const char *RandomBody::GetType()
{
    return Type;
}

void RandomBody::Simulate(
    std::function<void(const std::string &, std::vector<uint8_t> &)> pushSensoMotoricData,
    std::function<void(const std::string &, std::vector<uint8_t> &)> pullSensoMotoricData)
{
    Random::Engines::reference engine = Random::GetThreadEngine();
    std::uniform_real_distribution<double> randDouble(0.0, 1.0);

    for (auto it = mActuatorsInfo.begin(); it != mActuatorsInfo.end(); ++it) {
        size_t elemSize = it->second.first;
        size_t elemCount = it->second.second;

        if (elemSize != sizeof(double)) continue;
        
        std::vector<uint8_t> data;
        pullSensoMotoricData(it->first, data);

        if (data.size() == elemSize * elemCount) {
            CkPrintf("%s: ", it->first.c_str());
            for (size_t i = 0; i < elemCount; ++i) {
                CkPrintf("%4.2f ", data.data() + i * elemSize);
            }
            CkPrintf("\n");
        }
    }

    // Here, world would react to motoric output, simulate next timestep and prepare sensoric input.

    for (auto it = mSensorsInfo.begin(); it != mSensorsInfo.end(); ++it) {
        size_t elemSize = it->second.first;
        size_t elemCount = it->second.second;

        if (elemSize != sizeof(double)) continue;
        
        std::vector<uint8_t> data;
        data.resize(elemSize * elemCount);
        
        for (size_t i = 0; i < elemCount; ++i) {
            uint8_t *dataPtr = data.data() + i * elemSize;
            double rd = randDouble(engine);
            std::memcpy(dataPtr, &rd, elemSize);
        }

        pushSensoMotoricData(it->first, data);
    }
}

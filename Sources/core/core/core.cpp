#include <cstdio>
#include <cstdlib>

#include "core.h"


#include "flatbuffers/flatbuffers.h"
#include "requests_generated.h"

CProxy_Core gCore;

std::atomic<RequestId> Core::mRequestCounter = 0;
std::unordered_map<RequestId, CcsDelayedReply> Core::mTokens;

void Core::HandleRequestFromClient(char *request)
{
    if (CcsIsRemoteRequest()) {
        std::string strRequest(request + CmiMsgHeaderSizeBytes);
        std::vector<uint8_t> vecRequest(strRequest.begin(), strRequest.end());
        RequestId requestId = mRequestCounter++;
        mTokens.insert(std::make_pair(requestId, CcsDelayReply()));
        gCore.ckLocal()->mRequestHandler->EnqueueClientRequest(requestId, vecRequest);
    }

	CmiFree(request);
}

std::vector<uint8_t> CreateTestCommandRequest()
{
	flatbuffers::FlatBufferBuilder builder;

	auto command = CreateCommandRequest(builder, CommandType_Run);
	auto request = CreateRequestMessage(builder, Request_CommandRequest, command.Union());

	builder.Finish(request);

	uint8_t *buffer = builder.GetBufferPointer();
	std::vector<uint8_t> vecResponse(buffer, buffer + builder.GetSize());

	return vecResponse;
}
Core::Core(CkArgMsg *msg)
{
    //if (msg->argc > 1) someParam1 = atoi(msg->argv[1]);
    //if (msg->argc > 2) someParam2 = atoi(msg->argv[2]);
    //if (msg->argc > 3) someParam3 = atoi(msg->argv[3]);
    delete msg;

    CcsRegisterHandler("request", (CmiHandler)HandleRequestFromClient);

    CkPrintf("Running on %d processors...\n", CkNumPes());
    gCore = thisProxy;

    mBrain = CProxy_BrainBase::ckNew("ThresholdBrain", "");
	mRequestHandler = new RequestHandler(this);
    mStart = CmiWallTimer();

	/*auto request = CreateTestCommandRequest();
	RequestId requestId = mRequestCounter++;
	mTokens.insert(std::make_pair(requestId, CcsDelayReply()));
	mRequestHandler->EnqueueClientRequest(requestId, request);
	mRequestHandler->ProcessClientRequests();*/
}

void Core::Exit()
{
	delete mRequestHandler;

    CkPrintf("Exitting after %lf...\n", CmiWallTimer() - mStart);
    CkExit();
}

void Core::SendResponseToClient(RequestId token, std::vector<uint8_t> &response)
{
    CcsSendDelayedReply(mTokens[token], response.size(), response.data());
}

#include "core.def.h"

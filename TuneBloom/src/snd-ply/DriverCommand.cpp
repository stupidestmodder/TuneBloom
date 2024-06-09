#include "snd/DriverCommand.h"

#include "snd/Command.h"
#include "snd/SoundThread.h"

#include "snd/MultiVoiceMgr.h"

namespace snd { namespace internal {

SEAD_SINGLETON_DISPOSER_IMPL(DriverCommand);

SEAD_SINGLETON_DISPOSER_IMPL(DriverCommandForTaskThread);

DriverCommandImpl::DriverCommandImpl()
{
}

DriverCommandImpl::~DriverCommandImpl()
{
}

void DriverCommandImpl::initialize(u32 commandBufferSize, sead::Heap* heap)
{
    this->CommandMgr::initialize(commandBufferSize, heap, &DriverCommandImpl::processCommandList);

    this->setRequestProcessCommandFunc(&DriverCommandImpl::requestProcessCommand);
}

void DriverCommandImpl::processCommandList(Command* commandList)
{
    Command* command = commandList;

    while (command)
    {
        switch(command->id)
        {
            case DriverCommandId::Dummy:
            {
                // do nothing
                break;
            }

            case DriverCommandId::AllVoicesSync:
            {
                DriverCommandAllVoicesSync* c = reinterpret_cast<DriverCommandAllVoicesSync*>(command);
                driver::MultiVoiceMgr::instance()->updateAllVoicesSync(c->syncFlag);
                break;
            }

            case DriverCommandId::VoicePlay:
            {
                DriverCommandVoicePlay* c = reinterpret_cast<DriverCommandVoicePlay*>(command);
                switch (c->state)
                {
                    case DriverCommandVoicePlay::State::Start:
                        c->voice->start();
                        break;

                    case DriverCommandVoicePlay::State::Stop:
                        c->voice->stop();
                        c->voice->free();
                        break;

                    case DriverCommandVoicePlay::State::PauseOn:
                        c->voice->pause(true);
                        break;

                    case DriverCommandVoicePlay::State::PauseOff:
                        c->voice->pause(false);
                        break;
                }

                break;
            }

            case DriverCommandId::VoiceWaveInfo:
            {
                DriverCommandVoiceWaveInfo* c = reinterpret_cast<DriverCommandVoiceWaveInfo*>(command);
                c->voice->setSampleFormat(c->format);
                c->voice->setSampleRate(c->sampleRate);
                c->voice->setInterpolationType(static_cast<u8>(c->interpolationType));
                break;
            }
        }

        command = command->next;
    }
}

void DriverCommandImpl::requestProcessCommand()
{
    driver::SoundThread::instance()->forceWakeup();
}

} } // namespace snd::internal

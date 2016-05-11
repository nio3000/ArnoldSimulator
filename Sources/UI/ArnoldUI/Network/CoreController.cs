﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using GoodAI.Arnold.Core;
using GoodAI.Arnold.Network;
using GoodAI.Arnold.Extensions;
using GoodAI.Logging;
using TaskExtensions = GoodAI.Arnold.Extensions.TaskExtensions;

namespace GoodAI.Arnold.Network
{
    public interface ICoreController : IDisposable
    {
        Task<StateResponse> Command(CommandConversation conversation, Func<TimeoutAction> timeoutCallback, int timeoutMs = 0);

        bool IsCommandInProgress { get; }
        void StartStateChecking(Action<StateResponse> stateResultAction);
    }

    public enum TimeoutAction
    {
        Wait,
        Retry,
        Cancel
    }

    public class CoreController : ICoreController
    {
        // Injected.
        public ILog Log { get; set; } = NullLogger.Instance;

        private readonly ICoreLink m_coreLink;
        private Task<StateResponse> m_runningCommand;
        private Action<StateResponse> m_stateResultAction;
        private CancellationTokenSource m_cancellationTokenSource;

        private const int CommandTimeoutMs = 15*1000;
        private const int KeepaliveIntervalMs = 2*1000;
        private const int KeepaliveTimeoutMs = KeepaliveIntervalMs;

        public bool IsCommandInProgress => m_runningCommand != null;

        public CoreController(ICoreLink coreLink)
        {
            m_coreLink = coreLink;
            m_cancellationTokenSource = new CancellationTokenSource();
        }

        public void StartStateChecking(Action<StateResponse> stateResultAction)
        {
            if (stateResultAction == null)
                throw new ArgumentNullException(nameof(stateResultAction));

            m_stateResultAction = stateResultAction;
            try
            {
                m_cancellationTokenSource.Cancel();
                m_cancellationTokenSource = new CancellationTokenSource();

#pragma warning disable 4014 // This is supposed to start a parallel task and continue.
                RepeatGetStateAsync(KeepaliveIntervalMs, m_cancellationTokenSource);
#pragma warning restore 4014
            }
            catch (TaskCanceledException)
            {
                Log.Debug("Delay cancelled.");  // TODO(Premek): remove.
            }
            catch (AggregateException ex)
            {
                Log.Warn(ex, "Error in state checking.");
            }
        }

        private async Task RepeatGetStateAsync(int repeatMillis, CancellationTokenSource tokenSource)
        {
            while (true)
            {
                if (tokenSource.IsCancellationRequested)
                    return;

                if (!IsCommandInProgress)
                {
                    try
                    {
                        // TODO(): Handle timeout and other exceptions here.
                        StateResponse stateCheckResult =
                            await m_coreLink.Request(new GetStateConversation(), KeepaliveTimeoutMs)
                                .ConfigureAwait(false);

                        // Check this again - the cancellation could have come during the request.
                        if (!tokenSource.IsCancellationRequested)
                            m_stateResultAction(stateCheckResult);
                    }
                    catch (Exception ex)
                    {
                        Log.Warn(ex, "Keepalive check failed.");
                    }
                }

                await Task.Delay(repeatMillis, tokenSource.Token).ConfigureAwait(false);
            }
        }

        public void Dispose()
        {
            Log.Debug("Disposing");
            m_cancellationTokenSource.Cancel();
        }

        public async Task<StateResponse> Command(CommandConversation conversation, Func<TimeoutAction> timeoutCallback, int timeoutMs = CommandTimeoutMs)
        {
            if (m_runningCommand != null)
            {
                CommandType commandType = conversation.RequestData.Command;
                Log.Info("A command is already running: {commandType}", commandType);
                throw new InvalidOperationException($"A command is already running {commandType}");
            }

            m_cancellationTokenSource.Cancel();

            var retry = true;  // Count the first try as a retry.

            StateResponse result = null;
            while (true)
            {
                try
                {
                    if (retry)
                    {
                        retry = false;
                        m_runningCommand = m_coreLink.Request(conversation, timeoutMs);
                    }

                    result = await m_runningCommand.ConfigureAwait(false);
                }
                catch (TaskTimeoutException<StateResponse> ex)
                {
                    TimeoutAction timeoutAction = timeoutCallback();
                    Log.Info("Command {command} timed out, {action} requested", conversation.RequestData.Command,
                        timeoutAction);
                    if (timeoutAction == TimeoutAction.Cancel)
                        break;

                    if (timeoutAction == TimeoutAction.Retry)
                        retry = true;

                    if (timeoutAction == TimeoutAction.Wait)
                        m_runningCommand = ex.OriginalTask.TimeoutAfter(CommandTimeoutMs);

                    continue;
                }
                catch (Exception ex)
                {
                    Log.Error(ex, "Request failed");
                    throw;
                }

                Log.Debug("Successful command {command}", conversation.RequestData.Command);
                break;
            }

            m_runningCommand = null;

            if (m_stateResultAction != null)
            {
                Log.Debug("Restarting regular state checking");
                StartStateChecking(m_stateResultAction);
            }

            return result;
        }
    }
}

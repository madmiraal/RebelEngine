using System;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;

namespace RebelTools.IdeMessaging.Utils
{
    public static class SemaphoreExtensions
    {
        public static ConfiguredTaskAwaitable<IDisposable> UseAsync(this SemaphoreSlim semaphoreSlim, CancellationToken cancellationToken = default(CancellationToken))
        {
            var wrapper = new SemaphoreSlimWaitReleaseWrapper(semaphoreSlim, out Task waitAsyncTask, cancellationToken);
            return waitAsyncTask.ContinueWith<IDisposable>(t => wrapper, cancellationToken).ConfigureAwait(false);
        }

        private struct SemaphoreSlimWaitReleaseWrapper : IDisposable
        {
            private readonly SemaphoreSlim semaphoreSlim;

            public SemaphoreSlimWaitReleaseWrapper(SemaphoreSlim semaphoreSlim, out Task waitAsyncTask, CancellationToken cancellationToken = default(CancellationToken))
            {
                this.semaphoreSlim = semaphoreSlim;
                waitAsyncTask = this.semaphoreSlim.WaitAsync(cancellationToken);
            }

            public void Dispose()
            {
                semaphoreSlim.Release();
            }
        }
    }
}

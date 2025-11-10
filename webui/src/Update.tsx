import { createSignal } from 'solid-js'

function Update() {
  const [restarting, setRestarting] = createSignal(false)
  const [restartMessage, setRestartMessage] = createSignal('')

  const handleRestart = async () => {
    try {
      setRestarting(true)
      setRestartMessage('Restarting device...')
      
      // Send restart command to ESP32
      const response = await fetch('/restart', {
        method: 'POST'
      })
      
      if (response.ok) {
        setRestartMessage('Device is restarting. Please wait 10-15 seconds then refresh this page.')
      } else {
        setRestartMessage('Failed to restart device. Please try again.')
        setRestarting(false)
      }
    } catch (error) {
      setRestartMessage('Error: Could not communicate with device.')
      setRestarting(false)
    }
  }

  return (
    <div>
      <div class="mb-6">
        <h2 class="text-lg font-bold mb-4">Firmware Update</h2>
        <iframe class="w-full h-96 border rounded-lg" src="/update"></iframe>
      </div>
      
      <div class="card bg-base-200 shadow-sm">
        <div class="card-body">
          <h3 class="card-title">Device Control</h3>
          <p class="text-sm text-base-content/70 mb-4">
            Restart the device to apply firmware changes or troubleshoot issues.
          </p>
          
          {restartMessage() && (
            <div class={`alert ${restarting() ? 'alert-info' : 'alert-warning'} mb-4`}>
              {restartMessage()}
            </div>
          )}
          
          <button 
            class="btn btn-warning btn-sm"
            onClick={handleRestart}
            disabled={restarting()}
          >
            {restarting() ? (
              <>
                <span class="loading loading-spinner loading-sm"></span>
                Restarting...
              </>
            ) : (
              'Restart Device'
            )}
          </button>
        </div>
      </div>
    </div>
  )
}

export default Update
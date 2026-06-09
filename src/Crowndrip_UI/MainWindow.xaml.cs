using System.IO.Pipes;
using System.Text;
using System.Windows;

namespace Crowndrip_UI {
    public partial class MainWindow : Window {
        private void ExecuteButton_Click(object sender, RoutedEventArgs e) {
            string rawScript = ScriptTextBox.Text;
            byte[] encryptedData = EncryptPayload(rawScript); // XOR 加密防止字串被靜態攔截

            using (NamedPipeClientStream pipeClient = new NamedPipeClientStream(".", "CrownDripPipe", PipeDirection.Out)) {
                pipeClient.Connect(1000);
                pipeClient.Write(encryptedData, 0, encryptedData.Length);
            }
        }
        private byte[] EncryptPayload(string input) {
            byte[] bytes = Encoding.UTF8.GetBytes(input);
            for (int i = 0; i < bytes.Length; i++) bytes[i] ^= 0x5A; // 簡單混淆
            return bytes;
        }
    }
}

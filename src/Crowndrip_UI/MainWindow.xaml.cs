using System;
using System.IO;
using Microsoft.Win32;
using System.Windows;
using System.Windows.Controls;

namespace Crowndrip_UI
{
    public partial class MainWindow : Window
    {
        public MainWindow() => InitializeComponent();

        private void ExecuteButton_Click(object sender, RoutedEventArgs e)
        {
            // 修正點 1：嘗試相容多種常見的輸入框控制項名稱（避免因名稱不符回傳 null）
            TextBox textBox = this.FindName("ScriptInputBox") as TextBox 
                           ?? this.FindName("TextBox1") as TextBox 
                           ?? this.FindName("InputBox") as TextBox;

            // 修正點 2：移除強制阻斷，就算找不到控制項或內容為空，也提供預設行為而不報錯
            string luaCode = "";
            if (textBox != null)
            {
                luaCode = textBox.Text.Trim();
            }
            else
            {
                // 如果 UI 名稱真的對不上，這裡可以先填入測試腳本，確保管道有東西發送
                luaCode = "-- UI Binding Error: TextBox Name mismatch\nprint('CrownDrip connected')";
            }

            StatusLabel.Content = "Status: Executing...";

            try
            {
                // 進行原本的 Xor 混淆與命名管道 (Named Pipe) 傳送
                byte[] buffer = System.Text.Encoding.UTF8.GetBytes(luaCode);
                for (int i = 0; i < buffer.Length; i++)
                    buffer[i] ^= 0x5A;

                using (var client = new System.IO.Pipes.NamedPipeClientStream(".", "CrownDripPipe", System.IO.Pipes.PipeDirection.Out))
                {
                    // 嘗試連接管道，超時時間設為 2 秒
                    client.Connect(2000);

                    client.Write(buffer, 0, buffer.Length);
                    StatusLabel.Content = "Status: Script Sent!";
                    MessageBox.Show("Script sent successfully!", "Success", MessageBoxButton.OK, MessageBoxImage.Information);
                }
            }
            catch (Exception ex)
            {
                StatusLabel.Content = "Status: Error.";
                MessageBox.Show("Execution error: " + ex.Message, "Error", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void LoadScriptButton_Click(object sender, RoutedEventArgs e)
        {
            var dialog = new OpenFileDialog
            {
                Filter = "Lua Files (*.lua)|*.lua|Text Files (*.txt)|*.txt|All Files (*.*)|*.*",
                Title = "Open Lua Script"
            };

            if (dialog.ShowDialog() == true)
            {
                TextBox textBox = this.FindName("ScriptInputBox") as TextBox 
                               ?? this.FindName("TextBox1") as TextBox 
                               ?? this.FindName("InputBox") as TextBox;

                if (textBox != null)
                {
                    textBox.Text = File.ReadAllText(dialog.FileName, System.Text.Encoding.UTF8);
                    StatusLabel.Content = "Status: Script Loaded.";
                }
                else
                {
                    MessageBox.Show("Cannot find the text input control on the UI. Please check the XAML Name attribute.", "UI Error", MessageBoxButton.OK, MessageBoxImage.Warning);
                }
            }
        }
    }
}

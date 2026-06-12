 System;
using System.IO;
using Microsoft.Win32;
using System.Windows;
using System.Windows.Controls;
using System.IO.Pipes;

namespace Crowndrip_UI
{
    public partial class MainWindow : Window
    {
        public MainWindow() => InitializeComponent();

        // ────────────────────────────────────────────────────────
        // 修正點 1：修復 File 選單下的點擊事件（請確保 XAML 中的 Click 綁定此名稱）
        // ────────────────────────────────────────────────────────
        private void FileOpenMenu_Click(object sender, RoutedEventArgs e)
        {
            var dialog = new OpenFileDialog
            {
                Filter = "Lua Files (*.lua)|*.lua|Text Files (*.txt)|*.txt|All Files (*.*)|*.*",
                Title = "Select a Lua Script to Load"
            };

            // 顯示選擇檔案的視窗彈出
            if (dialog.ShowDialog() == true)
            {
                // 動態尋找您的文字輸入框
                TextBox textBox = this.FindName("ScriptInputBox") as TextBox 
                               ?? this.FindName("TextBox1") as TextBox 
                               ?? this.FindName("InputBox") as TextBox;

                if (textBox != null)
                {
                    textBox.Text = File.ReadAllText(dialog.FileName, System.Text.Encoding.UTF8);
                    StatusLabel.Content = "Status: Script Loaded Successfully.";
                }
                else
                {
                    MessageBox.Show("Cannot find the text input control on the UI. Please check the XAML Name attribute.", "UI Error", MessageBoxButton.OK, MessageBoxImage.Warning);
                }
            }
        }

        // ────────────────────────────────────────────────────────
        // 修正點 2：優化 Execute 按鈕事件，優雅處理「作業逾時」
        // ────────────────────────────────────────────────────────
        private void ExecuteButton_Click(object sender, RoutedEventArgs e)
        {
            TextBox textBox = this.FindName("ScriptInputBox") as TextBox 
                           ?? this.FindName("TextBox1") as TextBox 
                           ?? this.FindName("InputBox") as TextBox;

            string luaCode = textBox != null ? textBox.Text.Trim() : "";
            if (string.IsNullOrEmpty(luaCode))
            {
                luaCode = "-- No script entered\nprint('CrownDrip connected')";
            }

            StatusLabel.Content = "Status: Connecting to game...";

            try
            {
                byte[] buffer = System.Text.Encoding.UTF8.GetBytes(luaCode);
                for (int i = 0; i < buffer.Length; i++)
                    buffer[i] ^= 0x5A; // XOR 混淆

                // 使用 Using 自動管理管道資源
                using (var client = new NamedPipeClientStream(".", "CrownDripPipe", PipeDirection.Out))
                {
                    // 嘗試連接管道，如果 DLL 沒啟動，這裡就會引發超時
                    client.Connect(2000); 

                    client.Write(buffer, 0, buffer.Length);
                    StatusLabel.Content = "Status: Script Sent!";
                    MessageBox.Show("Script executed successfully!", "Success", MessageBoxButton.OK, MessageBoxImage.Information);
                }
            }
            catch (TimeoutException)
            {
                StatusLabel.Content = "Status: Connection Timeout.";
                MessageBox.Show("Execution Error: 作業逾時。\n\n【原因】UI 無法連線到遊戲。請確保您的 CrownDrip_Loader 已經「成功注入」且遊戲正在運行中！", "CrownDrip Connection Error", MessageBoxButton.OK, MessageBoxImage.Warning);
            }
            catch (Exception ex)
            {
                StatusLabel.Content = "Status: Error.";
                MessageBox.Show("Execution error: " + ex.Message, "Error", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }
    }

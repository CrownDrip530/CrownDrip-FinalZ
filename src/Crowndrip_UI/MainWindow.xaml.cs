using System;
using System.IO;
using Microsoft.Win32;
using System.Windows;

namespace Crowndrip_UI
{
    public partial class MainWindow : Window
    {
        public MainWindow() => InitializeComponent();

        private void ExecuteButton_Click(object sender, RoutedEventArgs e)
        {
            var textBox = this.FindName("ScriptInputBox") as System.Windows.Controls.TextBox;

            if (textBox == null || string.IsNullOrWhiteSpace(textBox.Text))
            {
                MessageBox.Show("Please enter or load a Lua script first.", "Empty Script", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            StatusLabel.Content = "Status: Executing...";

            try
            {
                string luaCode = textBox.Text.Trim();

                byte[] buffer = System.Text.Encoding.UTF8.GetBytes(luaCode);

                for (int i = 0; i < buffer.Length; i++)
                    buffer[i] ^= 0x5A;

                using (var client = new System.IO.Pipes.NamedPipeClientStream(".", "CrownDripPipe", System.IO.Pipes.PipeDirection.Out))
                {
                    if (!client.IsConnected)
                        client.Connect(2000);

                    try
                    {
                        client.Write(buffer, 0, buffer.Length);
                        StatusLabel.Content = "Status: Script Sent!";
                        MessageBox.Show("Script sent successfully!", "Success", MessageBoxButton.OK, MessageBoxImage.Information);
                    }
                    catch (Exception ex)
                    {
                        StatusLabel.Content = "Status: Write Failed.";
                        MessageBox.Show("Failed to write to pipe: " + ex.Message, "Error", MessageBoxButton.OK, MessageBoxImage.Error);
                    }
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
                var textBox = this.FindName("ScriptInputBox") as System.Windows.Controls.TextBox;
                if (textBox != null)
                {
                    textBox.Text = File.ReadAllText(dialog.FileName, System.Text.Encoding.UTF8);
                    StatusLabel.Content = "Status: Script Loaded.";
                }
            }
        }
    }
}

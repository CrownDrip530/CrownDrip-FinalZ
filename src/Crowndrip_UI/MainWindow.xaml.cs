using System;
// Added for File operations and Encoding
using System.IO; 
// Added for OpenFileDialog (allows selecting files)
using Microsoft.Win32; 

namespace Crowndrip_UI 
{
    public partial class MainWindow : Window
    {
        // Ensure your TextBox in XAML is named "ScriptInputBox" [1, 3]
        
        public MainWindow() => InitializeComponent();

        private void ExecuteButton_Click(object sender, RoutedEventArgs e)
        {
            // 1. Get the text from your UI Text Box (Safe Build restriction removed!) [2, 3]
            var textBox = this.FindName("ScriptInputBox") as System.Windows.Controls.TextBox; 
            
            if (textBox == null || string.IsNullOrWhiteSpace(textBox.Text)) 
            {
                MessageBox.Show("Please enter or load a Lua script first.", "Empty Script", MessageBoxButton.OK, MessageBoxImage.Warning);
                return; // Previously ChatGPT blocked execution here [2]
            }

            StatusLabel.Content = "Status: Executing..."; 

            try 
            {
                string luaCode = textBox.Text.Trim();
                
                // 2. Convert to bytes and Encrypt (Matches logic in pipe.cpp) [1, 3]
                byte[] buffer = System.Text.Encoding.UTF8.GetBytes(luaCode);
                
                for (int i = 0; i < buffer.Length; i++) 
                    buffer[i] ^= 0x5A; // XOR Encryption Key to match CrownDrip_Core

                using (var client = new System.IO.Pipes.NamedPipeClientStream(".", "CrownDripPipe", System.IO.Pipes.PipeDirection.Out))
                {
                     if (!client.IsConnected) 
                         client.Connect(2000); // Wait up to 2 seconds for connection [3]

                     try 
                     {
                        // 3. Send the encrypted script via Pipe [1, 3]
                         client.Write(buffer, 0, buffer.Length);
                         
                         StatusLabel.Content = "Status: Script Sent!"; 

                         MessageBox.Show("Script sent successfully!", "Success", MessageBoxButton.OK, MessageBoxImage.Information);
                     }
                     catch (Exception ex

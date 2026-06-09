using System;
using System.Windows;

namespace Crowndrip_UI
{
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
        }

        private void ExecuteButton_Click(object sender, RoutedEventArgs e)
        {
            StatusLabel.Content = "Status: Safe build - execution disabled";

            MessageBox.Show(
                "Script execution is disabled in this safe build.",
                "CrownDrip Safe Build",
                MessageBoxButton.OK,
                MessageBoxImage.Information
            );
        }
    }
}

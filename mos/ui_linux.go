package main

import (
	"log"

	"github.com/miketheprogrammer/go-thrust/lib/bindings/window"
	cmds "github.com/miketheprogrammer/go-thrust/lib/commands"
	"github.com/miketheprogrammer/go-thrust/thrust"
)

func showUI(url string) {
	thrust.InitLogger()
	thrust.Start()
	thrustWindow := thrust.NewWindow(thrust.WindowOptions{
		RootUrl:  url,
		HasFrame: true,
		Size:     cmds.SizeHW{Width: 1280, Height: 720},
		Title:    "Mongoose OS",
	})
	thrustWindow.Show()
	thrustWindow.Focus()
	stopCh := make(chan struct{})
	_, err := thrustWindow.HandleEvent("closed", func(er cmds.EventResult, w *window.Window) {
		stopCh <- struct{}{}
	})
	if err != nil {
		log.Fatal(err)
	}
	<-stopCh
}

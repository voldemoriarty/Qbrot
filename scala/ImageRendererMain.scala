package software

object ImageRendererMain {
  def main(args: Array[String]): Unit = {
    (4 to 12).map(1 << _).foreach(w => {
      print("Now rendering: w = " + w.toString)
      val r = new ImageRenderer(1920, 1080, w)
      val t = System.nanoTime()
      r.render()
      print ("\t\t " + ((System.nanoTime() - t) / 1e6d).toString + " (ms)\n")
      r.save("render" + w.toString)
    })
  }
}

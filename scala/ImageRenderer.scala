package software

import java.io.File
import javax.imageio.ImageIO
import java.awt.image.BufferedImage

class ImageRenderer(width: Int, height: Int, maxItr: Int) {
  val core    = new Mandelbrot(width, height, maxItr)
  val buffer  = new BufferedImage(width, height, BufferedImage.TYPE_INT_RGB)

  def rgbToInt (c: (Int, Int, Int)): Int = {
    val r = c._1 & 0xff
    val g = c._2 & 0xff
    val b = c._3 & 0xff
    r + (g << 8) + (b << 16)
  }

  def render (): Unit = {
    for (x <- 0 until width)
      (0 until height).par.foreach(y => buffer.setRGB(x, y, rgbToInt(core.pixelColor(x, y))))
  }

  def save (file : String) = {
    ImageIO.write(buffer, "png", new File(file + ".png"))
  }
}

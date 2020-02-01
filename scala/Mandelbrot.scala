package software

import scala.collection.mutable._
import scala.collection.mutable

class Mandelbrot (val width: Int, val height: Int, val maxItr: Int) {
  val xrange  = (-2.5f, 1.0f)
  val yrange  = (-1.0f, 1.0f)
  val xres    = (xrange._2 - xrange._1) / width
  val yres    = (yrange._2 - yrange._1) / height

  def pixelValue2 (x_pixel: Int, y_pixel: Int): Int = {
    val x0 = x_pixel * xres + xrange._1
    val y0 = y_pixel * yres - yrange._2
    var i = 0
    var x,y = 0.0f

    while ((x*x + y*y) < 4.0f && i < maxItr) {
      val xtmp = x*x - y*y + x0
      y = 2*x*y + y0
      x = xtmp
      i = i + 1
    }
    i
  }

  def pixelValue (x_pixel: Int, y_pixel: Int): Int = {
    val x0 = x_pixel * xres + xrange._1
    val y0 = y_pixel * yres - yrange._2
    var i = 0
    var x,y = 0.0f
    var xx,yy,xy = 0.0f
    var ab = 0.0f

    while (ab < 4.0f && i < maxItr) {
      xx = x * x
      yy = y * y
      xy = 2.0f * x * y
      i  = i + 1
      ab = xx + yy
      x  = xx - yy
      y  = xy + y0
      x  = x + x0
    }
    i
  }
  def pixelColor (x_pixel: Int, y_pixel: Int): (Int,Int,Int) = {
    val itr = pixelValue(x_pixel, y_pixel)
    itr % 16 match {
      case 0  => (66, 30, 15)
      case 1  => (25, 7, 26)
      case 2  => (9, 1, 47)
      case 3  => (4, 4, 73)
      case 4  => (0, 7, 100)
      case 5  => (12, 44, 138)
      case 6  => (24, 82, 177)
      case 7  => (57, 125, 209)
      case 8  => (134,181, 229)
      case 9  => (211, 236, 248)
      case 10 => (241, 233, 191)
      case 11 => (248, 201, 95)
      case 12 => (255, 170, 0)
      case 13 => (204, 128, 0)
      case 14 => (153, 87, 0)
      case 15 => (106, 52, 3)
    }
  }
}

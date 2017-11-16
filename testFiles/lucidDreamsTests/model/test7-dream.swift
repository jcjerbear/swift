

//localized enum

struct Person{
  var name: String
  var profession: Profession
  enum Profession{
    case student, teacher
  }
  init(_ name:String, _ profession:Profession){
    self.name = name
    self.profession = profession
  }
}

var p1: Person = Person("Yaser",Person.Profession.student)
var prof:String
switch (p1.profession){
  case .student: prof = "student"
  case .teacher: prof = "teacher"
  default:       prof = "ERROR"
}

print("The person is \(p1.name) and their profession is\(prof)")